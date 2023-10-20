# lab2-doc

## comments

- 正常字符：直接通过调用`adjust()`处理即可
- 嵌套comment：
  需要使用comment_level进行处理，如果只是识别到`*/`就退出comment模式就会出错

~~~c
"/*" {adjust(); comment_level_++;}
"*/" {adjust(); comment_level_--; if(!comment_level_) begin(StartCondition__::INITIAL);}
~~~

## strings

- 正常字符: 直接调用adjustStr()，并将string_buf_加上`matched()`即可
- 转义字符：参考了 [Tiger Manual](https://www.lrde.epita.fr/~tiger/old/tiger-manual/tiger.html#Lexical-Specifications)上列出的转义和控制字符进行了特判
- 字符串中间换行：格式如下的字符串应该被拼接在一起，因此先识别`\`再识别若干空白字符最后再识别`\`，将识别到的内容直接丢弃
  
~~~c
"abcd\
\efgh"
~~~

- 形如`\^A`的字符，需要和1-26进行对应，需要增加一个判断内容
- 开始识别string的时候，需要讲string_buf_清空

## error handling

- `.` 的错误处理: 通过对定义的总结，发现只有在INITIAL状态下只有ID和RBRACK后面出现出现`.`才是合法的，因此在`scanner.h`中引入了`UpdateToken()`和`CheckDot()`的函数，每次更新最新识别的token时调用第一个函数，在遇到`.`的时候调用第二个函数，如果返回true则说明是合法的，否则就是非法的
- 在所有可识别符号的最后增加一行判断，将所有词法分析器无法识别的词统一归类为无法识别的token

~~~c
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
~~~

## end-of-file handling

~~~c
<<EOF>>{return 0;}
~~~

即可
