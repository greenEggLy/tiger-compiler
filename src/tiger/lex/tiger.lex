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
  "array" {adjust();UpdateToken(Parser::ARRAY); return Parser::ARRAY;}
  "if" {adjust(); UpdateToken(Parser::IF);return Parser::IF;}
  "then" {adjust();UpdateToken(Parser::THEN); return Parser::THEN;}
  "else" {adjust(); UpdateToken(Parser::ELSE);return Parser::ELSE;}
  "while" {adjust();UpdateToken(Parser::WHILE); return Parser::WHILE;}
  "for" {adjust();UpdateToken(Parser::FOR); return Parser::FOR;}
  "to" {adjust();UpdateToken(Parser::TO); return Parser::TO;}
  "do" {adjust();UpdateToken(Parser::DO); return Parser::DO;}
  "let" {adjust();UpdateToken(Parser::LET); return Parser::LET;}
  "in" {adjust();UpdateToken(Parser::IN); return Parser::IN;}
  "end" {adjust();UpdateToken(Parser::END); return Parser::END;}
  "of" {adjust();UpdateToken(Parser::OF); return Parser::OF;}
  "break" {adjust();UpdateToken(Parser::BREAK) ;return Parser::BREAK;}
  "nil" {adjust();UpdateToken(Parser::NIL); return Parser::NIL;}
  "function" {adjust();UpdateToken(Parser::FUNCTION); return Parser::FUNCTION;}
  "var" {adjust();UpdateToken(Parser::VAR); return Parser::VAR;}
  "type" {adjust();UpdateToken(Parser::TYPE); return Parser::TYPE;}

  /* operators */
  "+" {adjust();UpdateToken(Parser::PLUS); return Parser::PLUS;}
  "-" {adjust();UpdateToken(Parser::MINUS); return Parser::MINUS;}
  "*" {adjust();UpdateToken(Parser::TIMES); return Parser::TIMES;}
  "/" {adjust();UpdateToken(Parser::DIVIDE); return Parser::DIVIDE;}
  "=" {adjust();UpdateToken(Parser::EQ); return Parser::EQ;}
  "<>" {adjust();UpdateToken(Parser::NEQ); return Parser::NEQ;}
  "<" {adjust();UpdateToken(Parser::LT) ;return Parser::LT;}
  "<=" {adjust();UpdateToken(Parser::LE); return Parser::LE;}
  ">" {adjust();UpdateToken(Parser::GT); return Parser::GT;}
  ">=" {adjust();UpdateToken(Parser::GE); return Parser::GE;}
  "&" {adjust();UpdateToken(Parser::AND); return Parser::AND;}
  "|" {adjust();UpdateToken(Parser::OR); return Parser::OR;}
  ":=" {adjust();UpdateToken(Parser::ASSIGN); return Parser::ASSIGN;}

  /* delimiters */
  "." {
        adjust(); 
        if(!CheckDot())
          errormsg_->Error(errormsg_->tok_pos_, "illegal token");
        else{
          UpdateToken(Parser::DOT);
          return Parser::DOT;
        }
      }
  "," {adjust(); UpdateToken(Parser::COMMA);return Parser::COMMA;}
  ":" {adjust();UpdateToken(Parser::COLON); return Parser::COLON;}
  ";" {adjust();UpdateToken(Parser::SEMICOLON); return Parser::SEMICOLON;}
  "(" {adjust();UpdateToken(Parser::LPAREN); return Parser::LPAREN;}
  ")" {adjust();UpdateToken(Parser::RPAREN); return Parser::RPAREN;}
  "[" {adjust();UpdateToken(Parser::LBRACK); return Parser::LBRACK;}
  "]" {adjust();UpdateToken(Parser::RBRACK); return Parser::RBRACK;}
  "{" {adjust();UpdateToken(Parser::LBRACE); return Parser::LBRACE;}
  "}" {adjust();UpdateToken(Parser::RBRACE); return Parser::RBRACE;}

/* identifier */
  [a-zA-Z][a-zA-Z0-9_]* {adjust(); string_buf_ = matched();UpdateToken(Parser::ID); return Parser::ID;}

/* integer */
  [0-9]+ {adjust(); string_buf_ = matched();UpdateToken(Parser::INT); return Parser::INT;}
}
/* string */
<INITIAL> \" {adjust(); begin(StartCondition__::STR); flushBuffer();}
<STR> {
      \" {adjustStr(); setMatched(string_buf_);  begin(StartCondition__::INITIAL);UpdateToken(Parser::STRING); return Parser::STRING;}
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

<<EOF>> {return 0;}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
