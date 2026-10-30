# IOC Access Security

## ACF Syntax Forward Compatibility

EPICS 7.0.10 modified the Access Security Configuration File (ACF) parser to
standardize the ACF grammar for forward compatibility.
It did not change the syntax that was accepted by earlier versions of the parser,
so existing access security configuration files would not need to be modified.
All ACF definitions will adhere to a consistent syntax format,
which will allow future additions to the access security language
without breaking existing configurations.
In practice, this means the structure of ACF files is now formally defined
and will remain stable going forward,
so any new grammar features will fit into the same pattern.
(Existing ACF files continue to work as-is under the new parser,
so no changes are required for legacy configurations or tools.).

**Generic ACF Syntax:**
The ACF file consists of definitions for User Access Groups (UAG),
Host Access Groups (HAG),
and Access Security Groups (ASG),
using the following general format
(angle brackets below denote placeholders):

```text
UAG(<name>) [{ <user> [, <user> ...] }]
...

HAG(<name>) [{ <host> [, <host> ...] }]
...

ASG(<name>) [{
    [INP<index>(<pvname>)
     ...]

    RULE(<level>, NONE | READ | WRITE [, NOTRAPWRITE | TRAPWRITE]) {
        [UAG(<name> [, <name> ...])]
        [HAG(<name> [, <name> ...])]
        [CALC(<calculation>)]
    }
    ...
}]
...
```

Under this schema each definition comprises a keyword,
a name in parentheses,
and (optionally) a braced block of contents.
This uniform structure ensures that
**future keywords or sections**
can be introduced in the same form,
maintaining compatibility with the parser.
For example, if a new type of condition or group is added in a later release,
it would follow the `KEYWORD(name) { ... }` pattern,
so 7.0.10-era parsers can handle or ignore it gracefully
instead of failing on unknown syntax.

**Supported Syntax in EPICS 7.0.10:**
The current release defines the following specific elements
within the above generic format:

-   **UAG** -- *User Access Group*.
Defines a group of user names.
-   **HAG** -- *Host Access Group*.
Defines a group of host names
(or IP addresses) that clients can connect from.
-   **ASG** -- *Access Security Group*.
Defines a security group which records can be assigned to.
An ASG entry may contain a block with input definitions and access rules.
For example:

```text
ASG(MyGroup) {
    INPA(myPV1)
    INPB(myPV2)
    RULE(1, WRITE) { ... }
    RULE(1, READ) { ... }
}
```

If no rules are defined for an ASG,
the access permissions default to always allowed.

-   **INP<index>(<pvname>)** -- *Input link*.
Declares an input process variable whose value can be used in a CALC condition.
-   **RULE(<level>, <permission> [, <logOption>]) { ... }** --
Defines an access rule for the ASG.

Inside the curly braces of a RULE,
**optional conditions** can restrict when that rule applies.
All conditions that are present must be satisfied
(they function as a logical AND):

-   **UAG(<name>, ...)** -- User-group condition.
The rule only applies if the Channel Access client's user
is a member of one of the listed UAGs.
-   **HAG(<name>, ...)** -- Host-group condition.
The rule only applies if the client's host
(as determined by its IP or hostname) is in one of the listed HAGs
-   **CALC("<expression>")** -- Calculation condition.
The rule only applies if the given expression evaluates to true (non-zero).

**Special Semantics for RULEs:**
Rules will continue to allow the prescribed access if and only if
all the predicates the rule contains are satisfied.
- If the rule contains predicates that are unknown to the parser
(indicating future functionality),
then the rule will NOT not match,
but no syntax error will be reported as long as the syntax is correct.
- If the rule contains predicates that the parser does not recognise
which are malformed (e.g. missing parentheses),
then the rule will not match and the parser will report a syntax error.
- In this way rules can be extended with new predicates
without breaking older clients or giving those older clients elevated privileges.

**Special Semantics for unrecognised ACF file elements:**
Any elements that are included in an ACF file will be ignored silently
by a parser that does not understand them.
- If an element is seen in an ACF file that is not understood by the parser,
the parser will simply ignore it silently,
without reporting an error,
as long as its syntax is correct.
- If elements are added to the ACF file that are malformed
(e.g. missing parentheses),
the parser will report a syntax error.
- Thus new elements can be added to ACF files in new EPICS releases
without breaking older clients that loads those files.

In summary, ACF forward compatibility means that from EPICS 7.0.10 onward,
any new access security features will use this established syntax.
The parser will recognize new group types or rule options using the same
`<KEYWORD>(...) { ... }` convention,
ensuring they can be used in files loaded by IOCs running EPICS 7.0.10 or later
without being rejected by those IOCs or requiring their parser to be modified.
This change does not require any modifications to existing ACF files --
all legacy syntax remains valid,
and the new standardized grammar provides a robust foundation for future extensions.

---

## Full Language Specification for Access Security Configuration Files

### Lexical tokens

**Ignored stuff**

-   *Whitespace*: space, tab, `\r` (carriage return) -- may appear between tokens.

-   *Newlines*: `\n` -- same as whitespace for syntax, but tracked for error messages.

-   *Comments*: `#` ... end of line -- ignored.

**Terminals**

-   `UAG` → literal string `"UAG"`
-   `HAG` → `"HAG"`
-   `ASG` → `"ASG"`
-   `RULE` → `"RULE"`
-   `CALC` → `"CALC"`
-   `INP(link)` → literal `"INP"` followed immediately by one uppercase letter `A`-`U`

```text
    link-letter  ::= "A" | "B" | ... | "U"
    INP(link)    ::= "INP" link-letter
```

-   `INT` → integer literal

```text
    INT ::= [ "+" | "-" ]? DIGIT+
    DIGIT ::= "0" | "1" | ... | "9"
```

-   `FLOAT` → floating-point literal with decimal point

```text
    FLOAT ::= [ "+" | "-" ]? DIGIT* "." DIGIT+ [ ( "e" | "E" ) [ "+" | "-" ] DIGIT+ ]?
```

-   `STRING` → either
    -   **Unquoted**: One or more of

```text
        NAMECHAR ::= letter | digit | "_" | "-" | "+" | ":" | "." | "[" | "]" | "<" | ">" | ";"
        STRING(unquoted) ::= NAMECHAR+
```

    -   **Quoted**: Surrounding quotes are stripped;
        escapes are kept literal at parse level.

```text
        STRING(quoted) ::= '"' { STRINGCHAR | ESCAPE } '"'
        STRINGCHAR     ::= any char except '"' "\" "\n"
        ESCAPE         ::= "\" any-char
```

-   Punctuation tokens (single-character terminals):

```text
    "("  ")"  "{"  "}"  ","
```

---

### Grammar

#### Top level

```ebnf
acf-file ::= asconfig ;

asconfig ::= asconfig-item { asconfig-item } ;

asconfig-item ::=
      uag-def
    | hag-def
    | asg-def
    | generic-top-level-item ;
```

##### UAG / HAG groups

```ebnf
uag-def ::= "UAG" uag-head [ uag-body ] ;

uag-head ::= "(" STRING ")" ;

uag-body ::= "{" uag-user-list "}" ;

uag-user-list ::= STRING { "," STRING } ;
```

```ebnf
hag-def ::= "HAG" hag-head [ hag-body ] ;

hag-head ::= "(" STRING ")" ;

hag-body ::= "{" hag-host-list "}" ;

hag-host-list ::= STRING { "," STRING } ;
```

##### ASG (access security group)

```ebnf
asg-def ::= "ASG" asg-head [ asg-body ] ;

asg-head ::= "(" STRING ")" ;

asg-body ::= "{" asg-body-item { asg-body-item } "}" ;

asg-body-item ::=
      inp-config
    | rule-config ;
```

###### INP config

```ebnf
inp-config ::= INP(link) "(" STRING ")" ;`
```

###### RULE config

```ebnf
rule-config ::= "RULE" rule-head [ rule-body ] ;

rule-head ::=
      "(" rule-head-mandatory "," rule-log-option ")"
    | "(" rule-head-mandatory ")" ;

rule-head-mandatory ::= INT "," STRING ;

rule-log-option ::= STRING ;
```

```ebnf
rule-body ::= "{" rule-list "}" ;

rule-list ::= rule-list-item { rule-list-item } ;

rule-list-item ::=
      "UAG" "(" rule-uag-list ")"
    | "HAG" "(" rule-hag-list ")"
    | "CALC" "(" STRING ")"
    | rule-generic-block-elem ;
```

```ebnf
rule-uag-list ::= STRING { "," STRING } ;

rule-hag-list ::= STRING { "," STRING } ;`
```

##### Generic / future-proof syntax

These are the "catch-all" constructs that are **parsed** but currently **ignored** semantically.

###### Keyword classes

These are parser-level categories used inside generic constructs:

```ebnf
keyword ::=
      "UAG"
    | "HAG"
    | "CALC"
    | non-rule-keyword ;

non-rule-keyword ::=
      "ASG"
    | "RULE"
    | INP(link) ;   (* INPA..INPU *)
```

###### Generic head (argument list)

```ebnf
generic-head ::=
      "(" ")"
    | "(" generic-element ")"
    | "(" generic-list ")" ;

generic-list ::= generic-element "," generic-element { "," generic-element } ;

generic-element ::=
      keyword
    | STRING
    | INT
    | FLOAT ;
```

###### Generic blocks

```ebnf
generic-block ::=
      "{" generic-element "}"
    | "{" generic-list "}"
    | "{" generic-block-list "}" ;

generic-block-list ::= generic-block-elem { generic-block-elem } ;

generic-block-elem ::=
    generic-block-elem-name generic-head [ generic-block ] ;

generic-block-elem-name ::= keyword | STRING ;
```

###### Generic top-level items

These are "unknown" top-level constructs, all of which are parsed and then ignored with a warning.

```ebnf
generic-top-level-item ::=
      STRING generic-head generic-list-block
    | STRING generic-head generic-block
    | STRING generic-head ;
```

where

```ebnf
generic-list-block ::=
    "{" generic-element "}" "{" generic-list "}" ;
```

###### Generic blocks inside RULE bodies

These are the "future predicates" in a RULE's body; they cause the RULE to be disabled with a warning, but they **must** still parse.

```ebnf
rule-generic-block-elem ::=
    rule-generic-block-elem-name generic-head [ generic-block ] ;

rule-generic-block-elem-name ::= non-rule-keyword | STRING ;
```
