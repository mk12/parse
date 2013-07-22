// Copyright 2013 Mitchell Kember. Subject to the MIT License.

// Package parse provides an abstraction for obtaining and parsing arguments in
// simple programs. It allows arguments to be passed either on the command line
// or via standard input, making it easy pipe data to the program in addition
// to specifying arguments manually. It also handles the usage message and the
// formatting of error messages.
package parse

import (
	"bufio"
	"fmt"
	"github.com/kless/term"
	"io"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"unicode"
)

func init() {
	// We are using log to present errors to the user, so extra information such
	// as the date and time are unnecessary.
	log.SetFlags(0)
	log.SetPrefix(programName + ": ")
}

// programName is the name of the executable file that contains the program.
var programName = filepath.Base(os.Args[0])

// usage is the program's usage message, a short string demonstrating to the
// user how the program should be invoked.
var usage = "<usage message missing>"

// SetUsage creates the usage message using args, which should contain the
// portion of the usage message that lists the arguments. This is typically a
// list of the argument names separated by spaces.
//
// For example, the sleep command found on Unix systems takes one argument, the
// number of seconds to sleep for. A sleep program using parse should call
// SetUsage("seconds"). This will produce "usage: sleep seconds".
func SetUsage(args string) {
	usage = strings.TrimRightFunc(strings.Join([]string{"usage:", programName,
		args}, " "), unicode.IsSpace)
}

// A Parser is a function that parses a string that is supposed to represent a
// particular data type. It returns an error when the string cannot be parsed. A
// nil parser is equivalent to a function that simply returns the passed string.
type Parser func(string) (interface{}, error)

// parsers is the list of Parser functions that will be used to parse the
// arguments that the program receives. Each element in order corresponds to a
// single argument, so the number of arguments expected by the program is
// len(parsers), unless it is equal to one and repeat is true.
var parsers = []Parser{nil}

// repeat allows parsers to parse an arbitrary amount of arguments with a single
// function. If len(parsers) == 1 and repeat is true, then the program will
// accept any nonzero number of arguments of the same type. If repeat is false,
// it will accept only one argument.
var repeat = true

// SetEveryParser assigns p to be used to parse the program's arguments. The
// function passed to Main must be prepared to receive any nonzero number of
// arguments (but they are guaranteed to be of the type that p returns).
func SetEveryParser(p Parser) {
	parsers = []Parser{p}
	repeat = true
}

// SetParsers assigns ps to be used to parse the program's arguments. The
// function passed to Main is guaranteed to receive len(ps) arguments. Each
// Parser in ps corresponds to a single argument, so, for example, if the third
// parses an int, then the third argument received by the program is guaranteed
// to be an int.
func SetParsers(ps ...Parser) {
	parsers = ps
	repeat = false
}

// Int is a Parser that parses a string as an int.
var Int = Parser(func(s string) (interface{}, error) {
	n, err := strconv.ParseInt(s, 0, 0)
	if err != nil {
		return nil, err.(*strconv.NumError).Err
	}
	return int(n), nil
})

// Float64 is a Parser that parses a string as a float64.
var Float64 = Parser(func(s string) (interface{}, error) {
	n, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return nil, err.(*strconv.NumError).Err
	}
	return n, nil
})

// Restrict creates a new Parser by restricting p with the predicate function
// pred. If the string is parsed without error by p, the value will be passed on
// to pred, which return an error for invalid parsed values not covered by p.
// For example, Restrict can be used to return an error if a well-formed string
// parsed as an integer is negative (when negative inputs do not make sense).
func (p Parser) Restrict(pred func(interface{}) error) Parser {
	return func(s string) (interface{}, error) {
		x, err := p(s)
		if err != nil {
			return nil, err
		}
		if err := pred(x); err != nil {
			return nil, err
		}
		return x, nil
	}
}

// AssertInts converts a list of interface{} to a list of ints using a type
// assertion for each element. It is useful when combined with
// parse.SetEveryParser(parse.Int).
func AssertInts(args []interface{}) []int {
	ints := make([]int, len(args))
	for i := range args {
		ints[i] = args[i].(int)
	}
	return ints
}

// AssertFloat64s converts a list of interface{} to a list of float64s using a
// type assertion for each element. It is useful when combined with
// parse.SetEveryParser(parse.Float64).
func AssertFloat64s(args []interface{}) []float64 {
	floats := make([]float64, len(args))
	for i := range args {
		floats[i] = args[i].(float64)
	}
	return floats
}

// apply parses args and, if no errors were encountered, calls fn with them and
// returns true. If there were errors, it prints them and returns false. The
// length of args must not exceed that of parsers unless repeat is true.
func apply(fn func([]interface{}), args []string) bool {
	success := true
	parsed := make([]interface{}, len(args))
	for i, arg := range args {
		p := parsers[0]
		if !repeat {
			p = parsers[i]
		}
		var err error
		parsed[i], err = p(arg)
		if err != nil {
			success = false
			log.Printf("%s: %s\n", arg, err)
		}
	}
	if success {
		fn(parsed)
	}
	return success
}

// Main takes a function fn and applies it to a list of arguments, which comes
// from either the command line or from standard input depending on how the
// program is invoked.
//
// The function fn can safely use type assertions on the arguments passed to it
// as long as they match the types that are returned by the parser(s) passed to
// SetEveryParser or SetParsers. If neither of those functions were called, the
// arguments will all be strings. If an argument can be valid for the parser but
// invalid for the program, a custom Parser should be written or an existing one
// should be modified using Restrict (don't print error messages from fn).
//
// When the program is invoked with "-h" or "--help", the usage message will be
// printed to standard output. When invoked directly with the wrong number of
// arguments, the usage message will be printed to standard error. When the only
// argument is "-", or when there are none and input is piped or redirected, the
// arguments will be read and parsed from a line of standard input in a loop
// until an EOF is encountered (each line is like a separate invocation of fn).
// When invoked with the correct number of arguments, they will be parsed and
// passed to fn.
func Main(fn func([]interface{})) {
	args := os.Args[1:]
	switch {
	case len(args) == 1 && (args[0] == "-h" || args[0] == "--help"):
		fmt.Println(usage)
	case len(args) == 1 && args[0] == "-":
		log.SetPrefix("error: ")
		fallthrough
	case len(args) == 0 && !term.IsTerminal(term.InputFD):
		mapLines(fn)
	case repeat && len(args) > 0, !repeat && len(args) == len(parsers):
		if !apply(fn, args) {
			os.Exit(1)
		}
	default:
		log.SetPrefix("")
		log.Println(usage)
		os.Exit(1)
	}
}

// mapLines reads one line at a time from standard input, splits the line into
// tokens, parses them, and passes them to fn. Before returning, it calls
// os.Exit with exit status 1 if any of the input lines had the wrong number of
// arguments or if there were any parse errors.
func mapLines(fn func([]interface{})) {
	success := true
	scanner := newLineScanner(os.Stdin)
	for scanner.Scan() {
		args := tokenize(scanner.Bytes())
		switch {
		case !repeat && len(args) < len(parsers):
			success = false
			log.Println("too few arguments")
		case !repeat && len(args) > len(parsers):
			success = false
			log.Println("too many arguments")
		default:
			if !apply(fn, args.strings()) {
				success = false
			}
		}
	}

	if err := scanner.Err(); err != nil {
		success = false
		log.Println(err)
	}
	if !success {
		os.Exit(1)
	}
}

// newLineScanner returns a new bufio.Scanner that scans from r one line at a
// time. It will scan multi-line tokens if newlines are escaped with a backslash
// or if they are surrounded by quotation marks.
func newLineScanner(r io.Reader) *bufio.Scanner {
	scanner := bufio.NewScanner(lineReader{r})
	scanner.Split(scanLines)
	return scanner
}

// lineReader is a wrapper for another io.Reader object. It removes escaped
// newlines (a backslash followed by a newline) from its source reader.
type lineReader struct {
	source io.Reader
}

func (r lineReader) Read(data []byte) (n int, err error) {
	n, err = r.source.Read(data)
	shift := 0
	escaped := false
	for i, c := range data[:n] {
		if escaped && c == '\n' {
			shift += 2
			n -= 2
		} else {
			data[i-shift] = c
		}
		// An unescaped backslash escapes the next character.
		escaped = !escaped && c == '\\'
	}
	return
}

// dropCR drops a terminal carriage return from the data.
func dropCR(data []byte) []byte {
	if len(data) > 0 && data[len(data)-1] == '\r' {
		return data[0 : len(data)-1]
	}
	return data
}

// scanLines is a split function similar to bufio.ScanLines, except that
// newlines found inside pairs of single or double quotation marks will not
// terminate the token.
func scanLines(data []byte, atEOF bool) (advance int, token []byte, err error) {
	if atEOF && len(data) == 0 {
		return
	}
	escaped := false
	quote := byte(0)
	for i, c := range data {
		if quote == 0 {
			if c == '\n' {
				return i + 1, dropCR(data[:i]), nil
			}
			if !escaped && (c == '\'' || c == '"') {
				quote = c
			}
		} else if !escaped && c == quote {
			quote = 0
		}
		// An unescaped backslash escapes the next character.
		escaped = !escaped && c == '\\'
	}
	// If we're at EOF, we have a final, non-terminated line. Return it.
	if atEOF {
		return len(data), dropCR(data), nil
	}
	// Request more data.
	return 0, nil, nil
}

// A tokenList is a list of tokens. It acts as a slice of mutable strings.
type tokenList [][]byte

// strings returns the tokens of the tokenList converted to strings.
func (t tokenList) strings() []string {
	tokens := make([]string, len(t))
	for i, token := range t {
		tokens[i] = string(token)
	}
	return tokens
}

// countMaxTokens counts the maximum number of tokens for which the function
// tokenize must be prepared to allocate memory. Because it ignores backslashes
// and quotation marks, the actual number of tokens may be less.
func countMaxTokens(data []byte) int {
	n := 0
	wasSpace := true
	for _, c := range data {
		space := unicode.IsSpace(rune(c))
		if wasSpace && !space {
			n++
		}
		wasSpace = space
	}
	return n
}

// tokenize splits data around each instance of one or more consecutive
// whitespace characters, as defined by unicode.IsSpace (but only for characters
// represented by a single byte), returning the list of tokens. It attempts to
// mimic the way command-line arguments are tokenized in shell programs.
//
// A whitespace character preceded by a backslash or enclosed in single or
// double quotation marks does not count as a token separator. All backslashes
// and quotation marks are excluded from the returned tokens unless escaped with
// a backslash. They will also be removed from the data array.
func tokenize(data []byte) tokenList {
	tokens := make(tokenList, 0, countMaxTokens(data))
	start := -1 // start index for token in data
	shift := 0  // for deleting characters
	wasSpace := true
	escaped := false
	quote := byte(0)
	for i, c := range data {
		del := false
		if !escaped {
			if quote == 0 {
				if c == '\'' || c == '"' {
					quote = c
					del = true
				}
				space := unicode.IsSpace(rune(c))
				if wasSpace && !space {
					start = i - shift
				} else if !wasSpace && space {
					tokens = append(tokens, data[start:i-shift])
					start = -1
				}
				wasSpace = space
			} else if c == quote {
				quote = 0
				del = true
			}
		}
		// An unescaped backslash escapes the next character.
		escaped = !escaped && c == '\\'
		// Delete unescaped backslashes or quotation marks.
		if escaped || del {
			shift++
		} else {
			data[i-shift] = c
		}
	}
	// We have a final word with no space after it. Append it.
	if start != -1 {
		tokens = append(tokens, data[start:len(data)-shift])
	}
	return tokens
}
