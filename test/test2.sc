; 0) load library
use ./dlib1.so

; 1) check that indentation is ignored
    call dlib1_sym1

; 2) check that spaces at the end of the line are ignored
        call dlib1_sym2   

; 3) check that two commands at one line will raise warning (first command is executed)
            call dlib1_sym3     call dlib1_sym2

; 4) check that command with no argument will raise warning
use
call

use ./dlib2.so
    call dlib2_sym1
    call dlib2_sym2
    call dlib2_sym3

; 5) check that symbols with similar names in both libraries won't lead to error (different namespaces used, first match is called)
call dlib_common_sym

; 6) check that calling non-existant symbol raises error and stops execution
call SOMETHING2

call dlib1_sym1
