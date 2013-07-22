Parse
=====

Get rid of parsing boilerplate,
And make your program a shining UNIX citizen for free!

Want to **Go** and write some quick programs (solutions to [CCC][] problems, perhaps) without dealing with boring parsing code or even figuring out where to get the arguments from in the first place? Parse has got you covered.

What are you waiting for?

	$ go get github.com/mk12/parse

[CCC]: http://www.cemc.uwaterloo.ca/contests/computing.html

Features
--------

- abstracts away the source of program arguments
- provides a flexible model for parsing said arguments
- allows you to add custom parsers for your own data types
- facilitates piping, input redirection, and interactive use
- creates and displays a standard usage message
- prints helpful, context-aware error messages

Usage
-----

A program that uses Parse is easy to use. Here are some ways that you can Meaner, the exampe program below:

    $ meaner --help
	usage: meaner number ...
	$ meaner 1 2 3
	2
	$ echo 1 1 2e50 | meaner
	6.666666666666668e+49
	$ echo -1 1 > data
	$ meaner < data
	0
	$ meaner a b
	meaner: a: invalid syntax
	meaner: b: invalid syntax

Example
-------

Meaner calculates the mean of a list of numbers. Read the documentation in [parse.go](parse.go) for information on the functions it uses.

```go
package main

import (
	"fmt"
	"github.com/mk12/parse"
)

func mean(numbers []float64) float64 {
	if len(numbers) == 0 {
		return 0
	}
	sum := 0.0
	for _, x := range numbers {
		sum += x
	}
	return sum / float64(len(numbers))
}

func main() {
	parse.SetUsage("number ...")
	parse.SetEveryParser(parse.Float64)
	parse.Main(func(args []interface{}) {
		fmt.Println(mean(parse.AssertFloat64s(args)))
	})
}
```

License
-------

Copyright © 2013 Mitchell Kember

Parse is available under the MIT License; see [LICENSE](LICENSE.md) for details.
