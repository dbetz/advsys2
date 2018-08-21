# advsys2
A major update to AdvSys switching to a C-like syntax.

- Implement the finally clause.
- Add comma operator.
- Add ternary operator.
- Fix bug where data objects can get zero as their address.
- Add character constants.
- Cleanup local vs global memory allocation.
- Free local memory after each function is compiled.
- Need a way to parse user input.
- Need to support word lists for NOUNS, VERBS, etc.
- Need to update the Spin version of the bytecode interpreter.

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

expr = expr

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

function()
function ( arg [, arg ]... )
array [ index ]

(expr)
var
object . property
[ object property ]
[ object property arg [, arg ]... ]
integer
"string"
