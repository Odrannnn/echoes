#ifndef _ESTATEMSG
#define _ESTATEMSG

// The message a state machine sends a state function.
enum EStateMsg {
  kStateMsg_Activate,
  kStateMsg_Update,
  kStateMsg_Deactivate,
};

#endif // _ESTATEMSG
