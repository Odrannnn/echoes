#include "Kyoto/CDvdRequest.hpp"

#include "Kyoto/CARAMManager.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

#ifdef TARGET_PC
#include <stdio.h>
#endif

CRealDvdRequest::~CRealDvdRequest() {
  if (!IsComplete()) {
    PostCancelRequest();
    WaitUntilComplete();
  }
  DVDClose(&mFileInfo);
}

void CRealDvdRequest::WaitUntilComplete() {
  while (!CRealDvdRequest::IsComplete()) {
  }
}

bool CRealDvdRequest::IsComplete() {
  s32 status = DVDGetCommandBlockStatus(&mFileInfo.cb);
  bool ret = false;
  if (status == DVD_STATE_END || status == DVD_STATE_CANCELED) {
    ret = true;
  }

#ifdef TARGET_PC
  // HOST DIAGNOSTIC: what `DVDGetCommandBlockStatus` actually answers for this request.
  //
  // Printed on a change of request *or* of status and never otherwise, because the pak pump
  // calls this thousands of times per pak and the interesting fact is "the status settled on
  // something that is not END/CANCELED and stayed there". The extra fields are the command
  // block's own contents, so "the request was created with no command block" and "the block
  // was filled but the drive never finished it" are distinguishable from one log line.
  // mwcceppc does not define TARGET_PC, so the matching build never sees any of this.
  {
    static const void* sLastReq = nullptr;
    static int sLastStatus = -1;
    if (sLastReq != static_cast< const void* >(this) || sLastStatus != status) {
      sLastReq = this;
      sLastStatus = status;
      printf("[dvdreq] CRealDvdRequest %p: GetCommandBlockStatus(cb)=%d -> IsComplete=%d; "
             "cb={state=%d cmd=%u off=%u len=%u addr=%p xfer=%u userData=%p cb=%p}\n",
             sLastReq, status, static_cast< int >(ret), static_cast< int >(mFileInfo.cb.state),
             mFileInfo.cb.command, mFileInfo.cb.offset, mFileInfo.cb.length, mFileInfo.cb.addr,
             mFileInfo.cb.transferredSize, mFileInfo.cb.userData,
             reinterpret_cast< const void* >(mFileInfo.cb.callback));
      fflush(nullptr);
    }
  }
#endif

  return ret;
}

void CRealDvdRequest::PostCancelRequest() { DVDCancelAsync(&mFileInfo.cb, nullptr); }

int CRealDvdRequest::GetMediaType() const { return 1; }

void CARAMDvdRequest::WaitUntilComplete() {
  if (CARAMManager::GetInvalidDMAHandle() == x4_dmaReq) {
    return;
  }

  CARAMManager::WaitForDMACompletion(x4_dmaReq);
  x4_dmaReq = CARAMManager::GetInvalidDMAHandle();
}

bool CARAMDvdRequest::IsComplete() {
  if (x4_dmaReq != CARAMManager::GetInvalidDMAHandle()) {
    if (!CARAMManager::IsDMACompleted(x4_dmaReq)) {
      return false;
    }

    x4_dmaReq = CARAMManager::GetInvalidDMAHandle();
  }

  return true;
}

void CARAMDvdRequest::PostCancelRequest() {}

int CARAMDvdRequest::GetMediaType() const { return 0; }
