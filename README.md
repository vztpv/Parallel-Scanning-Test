# Parallel-Scanning-Test
Parallel Scanning Test, unstable. has bugs..

# json style text

# type : 0 - quoted string, 1 - number, 2 - true, 3 - false, 4 - null
# no comment.
# "name always quoted." : value
# value : number(integer, float), quoted string, true, false, null, 
#      array, object .. 
# no comma
# utf-8 (simple bom check.)
# line info.

# struct Token { int64_t start; int64_t len; int64_t type; int64_t line; int64_t next; };
# struct TokenError { Token errToken; std::string err };

    "json file" = { 
      "\"wow\"" = [ 1 2 3 4 ]
    }


# mask = (token.start - (before.start + token.len)) & 1;  #  0 or 1 


    "\"\ u0000 \""


# token 후보
     " \ " \ u0000 \ " "  # token 후보
     1 2 1 2   0   2 1 1  # type
    (0 0 0 0   1   1 0 0) # mask
     1 0 1 0   1   0 1 0  # even : 실제 idx등을 고려하지 않을떄.
     1 0 1 0   0   0 1 0  # even
     0 1 0 1   1   1 0 1  # odd = ~even  
 
