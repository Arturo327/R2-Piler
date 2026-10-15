var a : i64 = 1; /* inline */
/**/
var b : i64 = /* mid */ 2;
/* first
   second */
var c : i64 = 3;
/* a * b * c */
var d : i64 = /* outer /* inner */ 4;
var e : i64 = 5; /*/ tricky */ var f : i64 = 6;
var g : i64 = 7; // /* line comment wins
