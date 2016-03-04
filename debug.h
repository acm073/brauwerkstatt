#ifndef __DEBUG_H
#define __DEBUG_H

#ifndef __DEBUG
#define debugnnl(M)
#define debug(M)
#else
#define debugnnl(M) Serial.print(M);
#define debug(M) Serial.println(M);
#endif

#endif /* __DEBUG_H */
