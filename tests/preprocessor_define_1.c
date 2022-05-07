// @run!
// !rc: 0

int wat();

#define THING2 return // hii
#define THING3 THING2 return
   #define CLOSE   }
#define OPEN (

int main OPEN) {
    #define THING THING2 0;
    THING
CLOSE
