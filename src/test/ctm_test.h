#ifndef CTM_TEST_H
#define CTM_TEST_H

#include "ctm.h"

#define CTM_UNIT_TEST(name) int name()
#define XXX_CTM_UNIT_TEST(name) int name()

/* If a test requires a realistic live environment, use these.
 */
int ctm_init();
void ctm_quit();

/* Logging.
 */

#define CTM_FAIL_BEGIN(fmt,...) { \
  printf("%s:%d: TEST FAILURE\n",__FILE__,__LINE__); \
  if (fmt&&fmt[0]) printf("%16s: "fmt"\n","Message",##__VA_ARGS__); \
}

#define CTM_FAIL_MORE(label,fmt,...) { \
  printf("%16s: "fmt"\n",label,##__VA_ARGS__); \
}

#define CTM_FAIL_END { \
  return -1; \
}

/* Assertions.
 */

#define CTM_ASSERT(condition,fmt,...) if (!(condition)) { \
  CTM_FAIL_BEGIN(fmt,##__VA_ARGS__) \
  CTM_FAIL_MORE("As written","%s",#condition) \
  CTM_FAIL_END \
}

#define CTM_ASSERT_NOT(condition,fmt,...) if (condition) { \
  CTM_FAIL_BEGIN(fmt,##__VA_ARGS__) \
  CTM_FAIL_MORE("As written","!(%s)",#condition) \
  CTM_FAIL_END \
}

#define CTM_ASSERT_CALL(call,fmt,...) { \
  int _result=(call); \
  if (_result<0) { \
    CTM_FAIL_BEGIN(fmt,##__VA_ARGS__) \
    CTM_FAIL_MORE("As written","%s",#call) \
    CTM_FAIL_MORE("Result","%d",_result) \
    CTM_FAIL_END \
  } \
}

#define CTM_ASSERT_INTS(a,b,fmt,...) { \
  int _a=(a),_b=(b); \
  if (_a!=_b) { \
    CTM_FAIL_BEGIN(fmt,##__VA_ARGS__) \
    CTM_FAIL_MORE("As written","%s == %s",#a,#b) \
    CTM_FAIL_MORE("Values","%d == %d",_a,_b) \
    CTM_FAIL_END \
  } \
}

#endif
