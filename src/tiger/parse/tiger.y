%filenames parser
%scanner tiger/lex/scanner.h
%baseclass-preinclude tiger/absyn/absyn.h

 /*
  * Please don't modify the lines above.
  */

%union {
  int ival;
  std::string* sval;
  sym::Symbol *sym;
  absyn::Exp *exp;
  absyn::ExpList *explist;
  absyn::Var *var;
  absyn::DecList *declist;
  absyn::Dec *dec;
  absyn::EFieldList *efieldlist;
  absyn::EField *efield;
  absyn::NameAndTyList *tydeclist;
  absyn::NameAndTy *tydec;
  absyn::FieldList *fieldlist;
  absyn::Field *field;
  absyn::FunDecList *fundeclist;
  absyn::FunDec *fundec;
  absyn::Ty *ty;
  }

%token <sym> ID
%token <sval> STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK
  LBRACE RBRACE DOT
  ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF
  BREAK NIL
  FUNCTION VAR TYPE

 /* token priority */

%left OR
%left AND
%nonassoc EQ NEQ LT LE GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS

%type <exp> exp expseq opexp recexp ifexp whileexp forexp callexp
%type <explist> actuals nonemptyactuals sequencing 
%type <var> lvalue one oneormore
%type <declist> decs decs_nonempty
%type <dec> decs_nonempty_s vardec
%type <efieldlist> rec rec_nonempty
%type <efield> rec_one
%type <tydeclist> tydec
%type <tydec> tydec_one
%type <fieldlist> tyfields tyfields_nonempty
%type <field> tyfield
%type <ty> ty
%type <fundeclist> fundec
%type <fundec> fundec_one

%start program

%%
program:  exp  {absyn_tree_ = std::make_unique<absyn::AbsynTree>($1);};

 /*
  * var
  */
lvalue:  ID  {
  $$ = new absyn::SimpleVar(scanner_.GetTokPos(), $1);}
  | lvalue DOT ID {$$ = new absyn::FieldVar(scanner_.GetTokPos(), $1, $3);}
  | lvalue LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), $1, $3);}
  | ID LBRACK exp RBRACK {$$ = new absyn::SubscriptVar(scanner_.GetTokPos(), new absyn::SimpleVar(scanner_.GetTokPos(), $1), $3);}
  ;
 
 /* 
  * expressions
  */
// SeqExp(int pos, ExpList *seq) : Exp(pos), seq_(seq)
expseq: sequencing {
  $$ = new absyn::SeqExp(scanner_.GetTokPos(), $1);}
  ;

sequencing: 
    exp SEMICOLON sequencing {$$ = $3->Prepend($1);}
  | exp COMMA sequencing {$$ = $3->Prepend($1);}
  | exp {$$ = new absyn::ExpList($1);}
  ;

opexp: 
    exp EQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::EQ_OP, $1, $3);}
  | exp NEQ exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::NEQ_OP, $1, $3);}
  | exp PLUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::PLUS_OP, $1, $3);}
  | exp MINUS exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::MINUS_OP, $1, $3);}
  | exp TIMES exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::TIMES_OP, $1, $3);}
  | exp DIVIDE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::DIVIDE_OP, $1, $3);}
  | exp LT exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::LT_OP, $1, $3);}
  | exp LE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::LE_OP, $1, $3);}
  | exp GT exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::GT_OP, $1, $3);}
  | exp GE exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::GE_OP, $1, $3);}
  | exp AND exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::AND_OP, $1, $3);}
  | exp OR exp {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::Oper::OR_OP, $1, $3);}
  | MINUS exp %prec UMINUS {$$ = new absyn::OpExp(scanner_.GetTokPos(), absyn::MINUS_OP, new absyn::IntExp(scanner_.GetTokPos(), 0), $2);}
  ;

recexp: ID LBRACE rec RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, $3);}
  |  ID LBRACE RBRACE {$$ = new absyn::RecordExp(scanner_.GetTokPos(), $1, new absyn::EFieldList());}
  ;

ifexp: 
    IF exp THEN exp ELSE exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, $6);}
  | IF exp THEN exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $2, $4, nullptr);}
  | IF LPAREN exp RPAREN THEN exp ELSE exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $3, $6, $8);}
  | IF LPAREN exp RPAREN THEN exp {$$ = new absyn::IfExp(scanner_.GetTokPos(), $3, $6, nullptr);}
  ;

whileexp:  WHILE exp DO exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $2, $4);}
  | WHILE LPAREN exp RPAREN DO exp {$$ = new absyn::WhileExp(scanner_.GetTokPos(), $3, $6);}
  ;

forexp: FOR ID ASSIGN exp TO exp DO exp {$$ = new absyn::ForExp(scanner_.GetTokPos(), $2, $4, $6, $8);}
  ;

callexp: ID LPAREN sequencing RPAREN {
    $$ = new absyn::CallExp(scanner_.GetTokPos(), $1, $3);}
  | ID LPAREN RPAREN {$$ = new absyn::CallExp(scanner_.GetTokPos(), $1, new absyn::ExpList());}
  ;

exp:  
    lvalue {$$ = new absyn::VarExp(scanner_.GetTokPos(), $1);}
  | NIL {$$ = new absyn::NilExp(scanner_.GetTokPos());}
  | INT {$$ = new absyn::IntExp(scanner_.GetTokPos(), $1);}
  | STRING {$$ = new absyn::StringExp(scanner_.GetTokPos(), $1);}
  | lvalue ASSIGN exp {$$ = new absyn::AssignExp(scanner_.GetTokPos(), $1, $3);}
  | BREAK {$$ = new absyn::BreakExp(scanner_.GetTokPos());}
  | LET decs IN expseq END {$$ = new absyn::LetExp(scanner_.GetTokPos(), $2, $4);}
  | ID LBRACK exp RBRACK OF exp {$$ = new absyn::ArrayExp(scanner_.GetTokPos(), $1, $3, $6);}
  | LPAREN RPAREN {$$ = new absyn::VoidExp(scanner_.GetTokPos());}
  | LPAREN exp RPAREN {$$ = $2;}
  | LPAREN sequencing RPAREN {$$ = new absyn::SeqExp(scanner_.GetTokPos(), $2);}
  | opexp | recexp | ifexp | whileexp | forexp | callexp
  ;




 /*
  * EFields
  */
// EFieldList *Prepend(EField *efield)
rec: rec_one COMMA rec {$$ = $3->Prepend($1);}
  | rec_one {$$ = new absyn::EFieldList($1);}
  ;

// EField(sym::Symbol *name, Exp *exp) : name_(name), exp_(exp)
rec_one: ID EQ exp {$$ = new absyn::EField($1, $3);}
  ;

 


 /* 
  * declarations
  */
decs: decs_nonempty_s decs {$$ = $2->Prepend($1);}
  | decs_nonempty_s {$$ = new absyn::DecList($1);}
  ;

decs_nonempty_s: tydec {$$ = new absyn::TypeDec(scanner_.GetTokPos(), $1);}
  | fundec {$$ = new absyn::FunctionDec(scanner_.GetTokPos(), $1);}
  | vardec
  ;


// VarDec(int pos, sym::Symbol *var, sym::Symbol *typ, Exp *init)
vardec: VAR ID COLON ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, $4, $6);}
  | VAR ID ASSIGN exp {$$ = new absyn::VarDec(scanner_.GetTokPos(), $2, nullptr, $4);}
  ;


// FunDecList(FunDec *fun_dec)
fundec: FUNCTION fundec_one fundec {$$ = $3->Prepend($2);}
  | FUNCTION fundec_one {$$ = new absyn::FunDecList($2);}
  ;

// FunDec(int pos, sym::Symbol *name, FieldList *params, sym::Symbol *result,
//         Exp *body)
fundec_one: 
     ID LPAREN tyfields RPAREN EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, $3, nullptr, $6);}
  |  ID LPAREN tyfields RPAREN COLON ID EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, $3, $6, $8);}
  |  ID LPAREN RPAREN COLON ID EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, new absyn::FieldList(), $5, $7);}
  |  ID LPAREN RPAREN EQ exp {$$ = new absyn::FunDec(scanner_.GetTokPos(), $1, new absyn::FieldList(), nullptr, $5);}
  ;


tydec:  tydec_one tydec {$$ = $2->Prepend($1); }
  | tydec_one {$$ = new absyn::NameAndTyList($1);}
  ;

tydec_one:  TYPE ID EQ ty {$$ = new absyn::NameAndTy($2, $4);}
  ;



 /* 
  * specific types
  */
ty: 
   ID {$$ = new absyn::NameTy(scanner_.GetTokPos(), $1);}
  | LBRACE tyfields RBRACE {$$ = new absyn::RecordTy(scanner_.GetTokPos(), $2);} 
  | ARRAY OF ID {$$ = new absyn::ArrayTy(scanner_.GetTokPos(), $3);}
  ;


 /* 
  * tyfields
  */
tyfields: tyfield COMMA tyfields {$$ = $3->Prepend($1);}
  | tyfield {$$ = new absyn::FieldList($1);}
  ;

tyfield:  ID COLON ID {$$ = new absyn::Field(scanner_.GetTokPos(), $1, $3);}
  ;