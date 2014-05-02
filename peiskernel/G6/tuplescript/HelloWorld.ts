#!/usr/local/bin/tuplescript
# HelloWorld.ts 
#
# This file serves as an example of how the tuplescript language
# can be used to implement some different PEIS services

# The obligatory HelloWorld example. Demonstrating how functions are defined, 
# how they are called and the two datatypes strings ("Hello") and atoms ('world)

defun hello(x) {echo ("*** Hello " x) newline}
hello 'world

# Example of recursion, if statements and arithmethics
defun fac(x) {
      if eq(x,1) {1}
      else { prod(x,fac minus(x,1))}
}
echo ("FAC 5 = " fac 5) newline

# This demonstrates how tuples from our own tuplespace is accessed
echo ("*** My name is " $kernel.name " and my id is " $kernel.id "\n") 

# Creates a subscription in the tuplespace
subscribe -1:kernel.name
sleep 3.0

# Find tupleview 
echo "Looking for tupleview: \n"
let tupleview false
for x in @-1:kernel.name {
  echo "x: " echo x newline
  if equal(.data x, "tupleview") { let tupleview .owner x }
}
if tupleview {
   echo "Tupleview is: " echo tupleview newline
   set tupleview:message "Hello World"
}

sleep 10
