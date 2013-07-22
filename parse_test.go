// Copyright 2013 Mitchell Kember. Subject to the MIT License.

package parse

import (
	"bytes"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"testing"
)

var usageTests = []struct {
	name  string
	args  string
	usage string
}{
	{"", "", "usage:"},
	{"1", "", "usage: 1"},
	{"prog", "one two three", "usage: prog one two three"},
	{"!!!", "abc ...", "usage: !!! abc ..."},
}

func TestUsage(t *testing.T) {
	for i, test := range usageTests {
		programName = test.name
		SetUsage(test.args)
		if usage != test.usage {
			t.Errorf("%d. programName = %q; SetUsage(%q)\nusage = %q\n"+
				"expected %q", i, test.name, test.args, usage, test.usage)
		}
	}
}

var parserTests = []struct {
	parser Parser
	name   string
	input  string
	value  interface{}
	fail   bool
}{
	{Int, "Int", "", nil, true},
	{Int, "Int", "1", 1, false},
	{Int, "Int", " -5 ", nil, true},
	{Int, "Int", "31872391273281937293", nil, true},
	{Float64, "Float64", "", nil, true},
	{Float64, "Float64", "a", nil, true},
	{Float64, "Float64", "5e7", 5e7, false},
	{Int.Restrict(positive), "Int.Restrict(positive)", "0", 0, false},
	{Int.Restrict(positive), "Int.Restrict(positive)", "-5", nil, true},
}

func positive(v interface{}) error {
	if v.(int) < 0 {
		return errors.New("cannot be negative")
	}
	return nil
}

func formatValue(v interface{}) string {
	if v == nil {
		return "nil"
	}
	return fmt.Sprintf("%s(%v)", reflect.TypeOf(v), v)
}

func formatFail(fail bool) string {
	if fail {
		return "err != nil"
	}
	return "err == nil"
}

func TestParsers(t *testing.T) {
	for i, test := range parserTests {
		value, err := test.parser(test.input)
		if value != test.value || (err != nil) != test.fail {
			t.Errorf("%d. %s(%q)\nreturned %s and %s\nexpected %s and %s",
				i, test.name, test.input, formatValue(value),
				formatFail(err != nil), formatValue(test.value),
				formatFail(test.fail))
		}
	}
}

var scanTests = []struct {
	input string
	lines []string
}{
	{"", []string{}},
	{"\n", []string{""}},
	{"\n\n ", []string{"", "", " "}},
	{"abcxyz\n", []string{"abcxyz"}},
	{`AbC123\` + "\n...", []string{"AbC123..."}},
	{`!@#$;\\` + "\ntest", []string{`!@#$;\\`, "test"}},
	{`TEST\\\` + "\n123", []string{`TEST\\123`}},
	{"he'llo\nhello'1\nabc\\'\n ", []string{"he'llo\nhello'1", "abc\\'", " "}},
	{"a\"\n\"'a", []string{"a\"\n\"'a"}},
}

func TestLineScanner(t *testing.T) {
	for i, test := range scanTests {
		scanner := newLineScanner(strings.NewReader(test.input))
		lines := make([]string, 0, len(test.lines))
		for scanner.Scan() {
			lines = append(lines, scanner.Text())
		}
		if err := scanner.Err(); err != nil {
			t.Errorf("%d. scanner.Err() returned %q", i, err)
		}
		for j, line := range lines {
			if j >= len(test.lines) || line != test.lines[j] {
				t.Errorf("%d. scanned %q\nreturned %#v\nexpected %#v",
					i, test.input, lines, test.lines)
				break
			}
		}
	}
}

var tokenizeTests = []struct {
	input  string
	tokens tokenList
}{
	{
		"",
		tokenList{},
	},
	{
		"one two 3 4 5",
		tokenList{[]byte("one"), []byte("two"), {'3'}, {'4'}, {'5'}},
	},
	{
		"\ta\nb\r" + `c\ c\ c '' "d \"d d"`,
		tokenList{{'a'}, {'b'}, []byte("c c c"), {}, []byte(`d "d d`)},
	},
	{
		`'"''"' "'"\ abcxyz\ "'"   '  ' \\`,
		tokenList{[]byte(`""`), []byte("' abcxyz '"), []byte("  "), {'\\'}},
	},
	{
		`\a\b\c\ 1 \  \ ' '\\`,
		tokenList{[]byte("abc 1"), {' '}, []byte(`  \`)},
	},
	{
		"' \t" + ` " ab z1 \'`,
		tokenList{[]byte(" \t" + ` " ab z1 '`)},
	},
	{
		`' ab1 [z] \'\' 4`,
		tokenList{[]byte(` ab1 [z] '' 4`)},
	},
}

func TestTokenize(t *testing.T) {
	for i, test := range tokenizeTests {
		tokens := tokenize([]byte(test.input))
		for j, token := range tokens {
			if j >= len(test.tokens) || !bytes.Equal(token, test.tokens[j]) {
				t.Errorf("%d. tokenize([]byte(%#q))\nreturned %v\nexpected %v",
					i, test.input, tokens, test.tokens)
				break
			}
		}
	}
}
