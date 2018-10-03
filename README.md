# advsys2
A major update to AdvSys switching to a C-like syntax.

- Need a way to parse user input.
- Need to support SYNONYMs
- Implement the finally clause.
- Cleanup local vs global memory allocation.
- Free local memory after each function is compiled.

Language syntax:

// comment

/* comment */

def const = expr ;

def function-name ()
{
  /* statements */
}

def function-name ( arg [ , arg ]... )
{
  /* statements */
}

var
    variable-def [ , variable-def ]... ;
    
variable-def:

    name [ = value ]

return;

return expr;

var-statement:

    var variable-def [ , variable-def ]... ;
    
variable-def:

    variable [ scalar-initializer ]
    variable '[' size ']' [ array-initializer ]
    
scalar-initializer:

    = constant-expr
    
array-initializer:

    = { constant-expr [ , constant-expr ]... }

object name {
    [ property-def ]...
}

property-def:

    property : value ;
    property : method ( arg [ , arg ]... ) { /* statements */ };
    
if ( expr ) statement

if ( expr ) statement else statement

for ( init-expr; test-expr; inc-expr ) statement

while ( test-expr ) statement

do statement while ( test-expr )

continue ;

break ;

{ statements }

try { statements } [ catch (name) { statements } ] [ finally { statements } ]

throw expr ;

print expr [ $|, expr ]... [ $ ] ;

expr = expr ? expr : expr

expr += expr
expr -= expr
expr *= expr
expr /= expr
expr %= expr
expr &= expr
expr |= expr
expr ^= expr
expr <<= expr
expr >>= expr

expr && expr
expr || expr

expr ^ expr
expr | expr
expr & expr

expr == expr
expr !=  expr

expr < expr
expr <= expr
expr >= expr
expr > expr

expr << expr
expr >> expr

expr + expr
expr - expr
expr * expr
expr / expr
expr % expr

- expr
~ expr
! expr
++expr
--expr
expr++
expr--

function ( )
function ( arg [ , arg ]... )
array [ index ]
array.byte [ index ]

(expr)
var
object . property
object . property ( )
object . property ( arg [ , arg ]... )
super . property ( )
super . property ( arg [ , arg ]... )
integer
"string"
'c'
