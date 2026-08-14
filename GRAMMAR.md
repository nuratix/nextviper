# NextViper Formal Grammar Specification

This document defines the formal grammar of the **NextViper** programming language in Extended Backus-Naur Form (EBNF).

---

## 1. Lexical Grammar

### 1.1 Characters & Whitespace
```ebnf
Whitespace  ::= [ \t\r\n]+
Comment     ::= SingleLineComment | MultiLineComment
SingleLineComment ::= '//' [^\n]*
MultiLineComment  ::= '/*' ( MultiLineComment | [^*] | '*'+ [^*/] )* '*'+ '/'
```

### 1.2 Identifiers & Keywords
```ebnf
Identifier  ::= ( [a-zA-Z_] ) [a-zA-Z0-9_]*
Keyword     ::= 'let' | 'mut' | 'fn' | 'return' | 'if' | 'else' | 'while'
              | 'for' | 'in' | 'loop' | 'break' | 'continue' | 'true'
              | 'false' | 'null' | 'nil' | 'match' | 'struct' | 'type'
              | 'import' | 'and' | 'or' | 'not'
```

### 1.3 Numeric Literals
```ebnf
IntegerLiteral ::= DecimalInteger | HexInteger | BinaryInteger | OctalInteger
DecimalInteger ::= [0-9] [0-9_]*
HexInteger     ::= '0' [xX] [0-9a-fA-F] [0-9a-fA-F_]*
BinaryInteger  ::= '0' [bB] [01] [01_]*
OctalInteger   ::= '0' [oO] [0-7] [0-7_]*

FloatLiteral   ::= [0-9] [0-9_]* '.' [0-9] [0-9_]* ([eE] [+-]? [0-9]+)?
                 | [0-9] [0-9_]* [eE] [+-]? [0-9]+
                 | '.' [0-9] [0-9_]*
```

### 1.4 String Literals
```ebnf
StringLiteral  ::= '"' DoubleQuotedChar* '"'
                 | "'" SingleQuotedChar* "'"
                 | 'r"' [^"]* '"'
                 | '"""' MultiLineStringChar* '"""'

EscapeSequence ::= '\' ( [nrt\\'"0abfv] | 'x' HexDigit{2} | 'u{' HexDigit{1,6} '}' | 'u' HexDigit{4} )
```

---

## 2. Syntactic Grammar (EBNF)

### 2.1 Program & Top-Level
```ebnf
Program    ::= Statement* EOF
Statement  ::= LetStatement
             | IfStatement
             | WhileStatement
             | ForInStatement
             | ReturnStatement
             | BreakStatement
             | ContinueStatement
             | BlockStatement
             | ExpressionStatement
```

### 2.2 Statements
```ebnf
LetStatement      ::= 'let' 'mut'? Identifier (':' TypeAnnotation)? ('=' Expression)? ';'?
ExpressionStatement ::= Expression ';'?
BlockStatement    ::= '{' Statement* '}'

IfStatement       ::= 'if' Expression ( BlockStatement | ':' Statement+ )
                      ( 'else' ( IfStatement | BlockStatement | ':' Statement+ ) )?

WhileStatement    ::= 'while' Expression ( BlockStatement | ':' Statement+ )
ForInStatement    ::= 'for' Identifier 'in' Expression ( BlockStatement | ':' Statement+ )

ReturnStatement   ::= 'return' Expression? ';'?
BreakStatement    ::= 'break' ';'?
ContinueStatement ::= 'continue' ';'?

TypeAnnotation    ::= Identifier ('[' TypeAnnotation ']')?
```

### 2.3 Expressions & Precedence Hierarchy

The table below outlines expression operator precedence from lowest (binding least tightly) to highest (binding most tightly):

| Precedence | Operator | Description | Associativity |
|---|---|---|---|
| 1 (Lowest) | `=`, `+=`, `-=`, `*=`, `/=`, `%=` | Assignment | Right |
| 2 | `\|>` | Pipeline | Left |
| 3 | `or`, `\|\|` | Logical OR | Left |
| 4 | `and`, `&&` | Logical AND | Left |
| 5 | `==`, `!=` | Equality | Left |
| 6 | `<`, `<=`, `>`, `>=` | Relational | Left |
| 7 | `..`, `..=` | Range | Non-assoc |
| 8 | `+`, `-` | Additive | Left |
| 9 | `*`, `/`, `%` | Multiplicative | Left |
| 10 | `**`, `^` | Power | Right |
| 11 | `!`, `not`, `-` (unary) | Unary prefix | Right |
| 12 (Highest)| `()`, `[]`, `.` | Call, Index, Member | Left |

### 2.4 Expression Rules
```ebnf
Expression     ::= Assignment
Assignment     ::= ( Primary '.' Identifier | Primary '[' Expression ']' | Identifier )
                   ( '=' | '+=' | '-=' | '*=' | '/=' | '%=' ) Assignment
                 | Pipeline

Pipeline       ::= LogicalOr ( '|>' CallExpr )*
LogicalOr      ::= LogicalAnd ( ( 'or' | '||' ) LogicalAnd )*
LogicalAnd     ::= Equality ( ( 'and' | '&&' ) Equality )*
Equality       ::= Relational ( ( '==' | '!=' ) Relational )*
Relational     ::= Range ( ( '<' | '<=' | '>' | '>=' ) Range )*
Range          ::= Additive ( ( '..' | '..=' ) Additive )?
Additive       ::= Multiplicative ( ( '+' | '-' ) Multiplicative )*
Multiplicative ::= Power ( ( '*' | '/' | '%' ) Power )*
Power          ::= Unary ( ( '**' | '^' ) Power )*

Unary          ::= ( '!' | 'not' | '-' ) Unary
                 | Postfix

Postfix        ::= Primary ( '(' ArgumentList? ')' | '[' Expression ']' | '.' Identifier )*
ArgumentList   ::= Expression ( ',' Expression )* ','?

Primary        ::= IntegerLiteral
                 | FloatLiteral
                 | StringLiteral
                 | 'true' | 'false' | 'null' | 'nil'
                 | Identifier
                 | ArrayLiteral
                 | ObjectLiteral
                 | '(' Expression ')'

ArrayLiteral   ::= '[' ( Expression ( ',' Expression )* ','? )? ']'
ObjectLiteral  ::= '{' ( ObjectEntry ( ',' ObjectEntry )* ','? )? '}'
ObjectEntry    ::= ( StringLiteral | Identifier ) ':' Expression
```
