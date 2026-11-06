" Vim syntax file
" Language: R2-Lang (.r2)

if exists("b:current_syntax")
  finish
endif

syntax keyword r2Statement fn return if elif else while for switch case default
syntax keyword r2Storage   var arr extern pub export type newtype struct union class static enum
syntax keyword r2Flow      defer new delete reset
syntax keyword r2Keyword   as sizeof import from
syntax keyword r2Type      i8 u8 i16 u16 i32 u32 i64 u64 char void ptr
syntax keyword r2Special   NULL
syntax match   r2Comment   "\v//.*$"
syntax region  r2Comment   start="/\*" end="\*/"
syntax match   r2Char      "\v'([^'\\]|\\.)'"
syntax region  r2String    start=+"+ skip=+\\"+ end=+"+
syntax match   r2Number    "\v\b(0[xX][0-9a-fA-F]+|0[oO][0-7]+|0[bB][01]+|\d+)[uU]?\b"
syntax match   r2Operator  "\v[+\-*/%=<>!&\|\^\@\$?]+"
syntax match   r2Delimiter "\v[\(\)\{\}\[\],;\:]"

highlight def link r2Statement Statement
highlight def link r2Storage   StorageClass
highlight def link r2Flow      Keyword
highlight def link r2Keyword   Keyword
highlight def link r2Type      Type
highlight def link r2Special   Special
highlight def link r2Comment   Comment
highlight def link r2Char      Character
highlight def link r2String    String
highlight def link r2Number    Number
highlight def link r2Operator  Operator
highlight def link r2Delimiter Delimiter

let b:current_syntax = "r2"
