/*
  ESP8266 Hello World urlencode by Steve Nelson
  URLEncoding is used all the time with internet urls. This is how urls handle funny characters
  in a URL. For example a space is: %20
  
  These functions simplify the process of encoding and decoding the urlencoded format.
  
  It has been tested on an esp12e (NodeMCU development board)
  This example code is in the public domain, use it however you want.

  Prerequisite Examples:
  https://github.com/zenmanenergy/ESP8266-Arduino-Examples/tree/master/helloworld_serial
*/


#include <WString.h>


String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (unsigned int i=0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' ') {
        encodedString+= '+';
      } 
      else if (isalnum(c)) {
        encodedString+=c;
      } 
      else {
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      // yield();
    }
    
    return encodedString;
}

