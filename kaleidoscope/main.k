def average(x y) (x + y) * 0.5;
def addition(x y) x + y;
def addone(x) x=x+1 : x;

#fibonacci
def fibi(x)
  var a = 1, b = 1, c in
  (for i = 3, i < x in
     c = a + b :
     a = b :
     b = c) :
  b;
