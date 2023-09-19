%filenames = "scanner"

/*
 * Please don't modify the lines above.
 */

/* You can add lex definitions here. */

%x  COMMENT STR  

%%

/*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

<INITIAL>{
  /* reserved words */
  "array" {adjust(); return Parser::ARRAY;}
  "if" {adjust(); return Parser::IF;}
  "then" {adjust(); return Parser::THEN;}
  "else" {adjust(); return Parser::ELSE;}
  "while" {adjust(); return Parser::WHILE;}
  "for" {adjust(); return Parser::FOR;}
  "to" {adjust(); return Parser::TO;}
  "do" {adjust(); return Parser::DO;}
  "let" {adjust(); return Parser::LET;}
  "in" {adjust(); return Parser::IN;}
  "end" {adjust(); return Parser::END;}
  "of" {adjust(); return Parser::OF;}
  "break" {adjust(); return Parser::BREAK;}
  "nil" {adjust(); return Parser::NIL;}
  "function" {adjust(); return Parser::FUNCTION;}
  "var" {adjust(); return Parser::VAR;}
  "type" {adjust(); return Parser::TYPE;}


  /* operators */
  "+" {adjust(); return Parser::PLUS;}
  "-" {adjust(); return Parser::MINUS;}
  "*" {adjust(); return Parser::TIMES;}
  "/" {adjust(); return Parser::DIVIDE;}
  "=" {adjust(); return Parser::EQ;}
  "<>" {adjust(); return Parser::NEQ;}
  "<" {adjust(); return Parser::LT;}
  "<=" {adjust(); return Parser::LE;}
  ">" {adjust(); return Parser::GT;}
  ">=" {adjust(); return Parser::GE;}
  "&" {adjust(); return Parser::AND;}
  "|" {adjust(); return Parser::OR;}
  ":=" {adjust(); return Parser::ASSIGN;}

  /* delimiters */
  "." {adjust(); return Parser::DOT;}
  "," {adjust(); return Parser::COMMA;}
  ":" {adjust(); return Parser::COLON;}
  ";" {adjust(); return Parser::SEMICOLON;}
  "(" {adjust(); return Parser::LPAREN;}
  ")" {adjust(); return Parser::RPAREN;}
  "[" {adjust(); return Parser::LBRACK;}
  "]" {adjust(); return Parser::RBRACK;}
  "{" {adjust(); return Parser::LBRACE;}
  "}" {adjust(); return Parser::RBRACE;}
}
/* identifier */
[a-zA-Z][a-zA-Z0-9_]* {adjust(); string_buf_ = matched(); return Parser::ID;}

/* integer */
[0-9]+ {adjust(); string_buf_ = matched(); return Parser::INT;}

/* string */
<INITIAL> \" {adjust(); begin(StartCondition__::STR); flushBuffer();}
<STR> {
      \" {adjustStr(); setMatched(string_buf_);  begin(StartCondition__::INITIAL); return Parser::STRING;}
      /* ‘\a’, ‘\b’, ‘\f’, ‘\n’, ‘\r’, ‘\t’, ‘\v’ */
      \\a {adjustStr(); string_buf_ += '\a';}
      \\b {adjustStr(); string_buf_ += '\b';}
      \\f {adjustStr(); string_buf_ += '\f';}
      \\n {adjustStr(); string_buf_ += '\n';}
      \\r {adjustStr(); string_buf_ += '\r';}
      \\t {adjustStr(); string_buf_ += '\t';}
      \\v {adjustStr(); string_buf_ += '\v';}

      \\\\ {adjustStr(); string_buf_ += '\\';}
      \\\" {adjustStr(); string_buf_ += '\"';}
      \\0 {adjustStr(); string_buf_ += '\0';}
      \\[ \t\n\f]+\\ {adjustStr();}
      \\\^[A-Z] {adjustStr(); string_buf_ += matched()[2] - 'A' + 1;}

      \\[0-9]+ {adjustStr(); string_buf_ += (char)strtol(matched().c_str() + 1, nullptr,10); }

      /* other chars */
      . {adjustStr(); string_buf_ += matched();}
}

/* comment */
"/*" { adjust();begin(StartCondition__::COMMENT); comment_level_ = 1;}
<COMMENT>{
  "/*" {adjust(); comment_level_++;}
  "*/" {adjust(); comment_level_--; if(!comment_level_) begin(StartCondition__::INITIAL);}
  \n {adjust(); errormsg_->Newline();}
  . {adjust();}
}


 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
<INITIAL>. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
