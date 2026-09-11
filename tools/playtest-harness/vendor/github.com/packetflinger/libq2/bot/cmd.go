package bot

import (
	"strings"
)

// Cmd is one console command parsed out of a server stufftext: the command
// name and its arguments, tokenized the way Quake II's Cmd_TokenizeString
// does.
//
// The fields are unexported because bot.go reaches them directly (sayFunc
// reverses c.arguments in place); everything outside the package goes through
// the accessors.
type Cmd struct {
	commandName string
	arguments   []string
}

// GetCommand returns the command name, lowercased -- Quake II's own dispatch
// is case-insensitive.
func (c Cmd) GetCommand() string {
	return strings.ToLower(c.commandName)
}

// GetFullCommand returns the command and its arguments as one string, which is
// what a diagnostic wants to print.
func (c Cmd) GetFullCommand() string {
	if len(c.arguments) == 0 {
		return c.commandName
	}
	return c.commandName + " " + strings.Join(c.arguments, " ")
}

// Argv returns argument i, or "" when there is no such argument. Note this is
// NOT Quake's Cmd_Argv numbering: argument 0 here is the first argument AFTER
// the command name, because that is how bot.go's setFunc and aliasFunc use it
// (`set name value` reads Argv(0) as the cvar and Argv(1) as the value).
func (c Cmd) Argv(i int) string {
	if i < 0 || i >= len(c.arguments) {
		return ""
	}
	return c.arguments[i]
}

// Args returns every argument after the command name.
func (c Cmd) Args() []string {
	return c.arguments
}

// ParseCmd splits a stufftext into commands and tokenizes each one.
//
// A server sends several commands in one stufftext, separated by ';' or by
// newlines, and any token may be double-quoted -- `set name "two words"`.
// Quotes are stripped; a ';' inside quotes is part of the token, which is why
// this cannot be a strings.Split.
func ParseCmd(s string) []Cmd {
	var out []Cmd

	for _, line := range splitCommands(s) {
		tokens := tokenize(line)
		if len(tokens) == 0 {
			continue
		}
		out = append(out, Cmd{
			commandName: tokens[0],
			arguments:   tokens[1:],
		})
	}
	return out
}

// splitCommands breaks a stufftext on ';' and newlines, respecting quotes.
func splitCommands(s string) []string {
	var out []string
	var cur strings.Builder
	quoted := false

	for _, r := range s {
		switch {
		case r == '"':
			quoted = !quoted
			cur.WriteRune(r)
		case !quoted && (r == ';' || r == '\n' || r == '\r'):
			out = append(out, cur.String())
			cur.Reset()
		default:
			cur.WriteRune(r)
		}
	}
	if cur.Len() > 0 {
		out = append(out, cur.String())
	}
	return out
}

// tokenize splits one command into tokens on whitespace, keeping quoted runs
// together and dropping the quotes.
func tokenize(s string) []string {
	var out []string
	var cur strings.Builder
	quoted, have := false, false

	flush := func() {
		if have {
			out = append(out, cur.String())
			cur.Reset()
			have = false
		}
	}

	for _, r := range s {
		switch {
		case r == '"':
			quoted = !quoted
			have = true // `""` is an empty token, not no token
		case !quoted && (r == ' ' || r == '\t'):
			flush()
		default:
			cur.WriteRune(r)
			have = true
		}
	}
	flush()
	return out
}
