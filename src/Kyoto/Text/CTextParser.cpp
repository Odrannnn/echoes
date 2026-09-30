#include "Kyoto/Text/CTextParser.hpp"

#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Text/CRasterFont.hpp"
#include "Kyoto/Text/CTextExecuteBuffer.hpp"
#include "Kyoto/Text/TextCommon.hpp"
#include "rstl/StringExtras.hpp"
#include "rstl/algorithm.hpp"

CTextParser::CTextParser(IObjectStore& store) : mObjectStore(store) {}

void CTextParser::ParseText(CTextExecuteBuffer& buffer, const wchar_t* str, int len,
                            const rstl::vector< rstl::pair< CAssetId, CAssetId > >* textureMap) {
  int begin = 0;
  int end = 0;
  while (str[end] && (len == -1 || end < len)) {
    if (str[end] == L'&') {
      if ((len == -1 || end + 1 < len) && str[end + 1] != L'&') {
        if (end > begin) {
          buffer.AddString(str + begin, end - begin);
        }
        ++end;
        begin = end;
        while ((len == -1 || end < len) && str[end] && str[end] != L';') {
          ++end;
        }
        ParseTag(buffer, str + begin, end - begin, textureMap);
        begin = end + 1;
      } else {
        buffer.AddString(str + begin, end + 1 - begin);
        end += 2;
        begin = end;
      }
    } else {
      ++end;
    }
  }
  if (end > begin) {
    buffer.AddString(str + begin, end - begin);
  }
}

CAssetId CTextParser::GetAssetIdFromString(
    const rstl::string& text, const rstl::vector< rstl::pair< CAssetId, CAssetId > >* textureMap) {
  rstl::wstring str = CStringExtras::ConvertToUNICODE(text);
  int id = (GetColorValue(str.data()) << 24) | (GetColorValue(str.data() + 2) << 16) |
           (GetColorValue(str.data() + 4) << 8) | GetColorValue(str.data() + 6);
  if (textureMap) {
    typedef rstl::pair< CAssetId, CAssetId > AssetPair;
    rstl::vector< AssetPair >::const_iterator search = rstl::binary_find(
        textureMap->begin(), textureMap->end(), static_cast< CAssetId >(id),
        rstl::pair_sorter_finder< AssetPair, rstl::less< CAssetId > >(rstl::less< CAssetId >()));
    if (search != textureMap->end()) {
      return search->second;
    }
  }
  return id;
}

TToken< CRasterFont > CTextParser::GetFont(const wchar_t* str, int len) {
  const CAssetId id = (static_cast< uint >(GetColorValue(str)) << 24) |
                      (GetColorValue(str + 2) << 16) | (GetColorValue(str + 4) << 8) |
                      GetColorValue(str + 6);
  return mObjectStore.GetObj(SObjectTag('FONT', id));
}

CFontImageDef
CTextParser::GetImage(const wchar_t* str, int len,
                      const rstl::vector< rstl::pair< CAssetId, CAssetId > >* textureMap) {
  // TODO: parse static/animated image tags, crop factors and remapped texture IDs.
  return CFontImageDef(rstl::vector< TToken< CTexture > >(), 0.f, CVector2f(1.f, 1.f));
}

uint CTextParser::HandleUserTag(CTextExecuteBuffer& buffer, const wchar_t* str, int len) {
  return 0;
}

void CTextParser::ParseTag(CTextExecuteBuffer& buffer, const wchar_t* str, int len,
                           const rstl::vector< rstl::pair< CAssetId, CAssetId > >* textureMap) {
  if (BeginsWith(str, len, L"font=")) {
    TToken< CRasterFont > font = GetFont(str + 5, len - 5);
    buffer.AddFont(font);
  } else if (BeginsWith(str, len, L"image=")) {
    CFontImageDef texture = GetImage(str + 6, len - 6, textureMap);
    buffer.AddImage(texture);
  } else if (BeginsWith(str, len, L"fg-color=")) {
    buffer.AddColor(kCT_Foreground, ParseColor(str + 9, len - 9));
  } else if (BeginsWith(str, len, L"main-color=")) {
    buffer.AddColor(kCT_Main, ParseColor(str + 11, len - 11));
  } else if (BeginsWith(str, len, L"geometry-color=")) {
    buffer.AddColor(kCT_Geometry, ParseColor(str + 11, len - 11));
  } else if (BeginsWith(str, len, L"outline-color=")) {
    buffer.AddColor(kCT_Outline, ParseColor(str + 14, len - 14));
  } else if (BeginsWith(str, len, L"color")) {
    int idx = str[6] - L'0';
    if (idx < 0 || idx > 9) {
      return;
    }
    const wchar_t* str_remain = str + 7;
    len -= 7;
    if (*str_remain >= L'0' && *str_remain <= L'9') {
      wchar_t tmp = *str_remain;
      ++str_remain;
      len--;
      idx = (idx * 10) + (tmp - L'0');
    }
    if (Equals(str_remain + 10, len - 10, L"no")) {
      buffer.AddRemoveColorOverride(idx);
    } else {
      buffer.AddColorOverride(idx, ParseColor(str_remain + 10, len - 10));
    }
  } else if (BeginsWith(str, len, L"line-spacing=")) {
    const float v = (float)ParseInt(str + 13, len - 13, true);
    buffer.AddLineSpacing(v / 100.f);
  } else if (BeginsWith(str, len, L"line-extra-space=")) {
    buffer.AddLineExtraSpace(ParseInt(str + 17, len - 17, true));
  } else if (BeginsWith(str, len, L"character-extra-space=")) {
    buffer.AddCharacterExtraSpace(ParseInt(str + 22, len - 22, true));
  } else if (BeginsWith(str, len, L"just=")) {
    if (Equals(str + 5, len - 5, L"left")) {
      buffer.AddJustification(kJustification_Left);
    } else if (Equals(str + 5, len - 5, L"center")) {
      buffer.AddJustification(kJustification_Center);
    } else if (Equals(str + 5, len - 5, L"right")) {
      buffer.AddJustification(kJustification_Right);
    } else if (Equals(str + 5, len - 5, L"full")) {
      buffer.AddJustification(kJustification_Full);
    } else if (Equals(str + 5, len - 5, L"nleft")) {
      buffer.AddJustification(kJustification_NLeft);
    } else if (Equals(str + 5, len - 5, L"ncenter")) {
      buffer.AddJustification(kJustification_NCenter);
    } else if (Equals(str + 5, len - 5, L"nright")) {
      buffer.AddJustification(kJustification_NRight);
    }
  } else if (BeginsWith(str, len, L"vjust=")) {
    if (Equals(str + 6, len - 6, L"top")) {
      buffer.AddVerticalJustification(kVerticalJustification_Top);
    } else if (Equals(str + 6, len - 6, L"center")) {
      buffer.AddVerticalJustification(kVerticalJustification_Center);
    } else if (Equals(str + 6, len - 6, L"bottom")) {
      buffer.AddVerticalJustification(kVerticalJustification_Bottom);
    } else if (Equals(str + 6, len - 6, L"full")) {
      buffer.AddVerticalJustification(kVerticalJustification_Full);
    } else if (Equals(str + 6, len - 6, L"ntop")) {
      buffer.AddVerticalJustification(kVerticalJustification_NTop);
    } else if (Equals(str + 6, len - 6, L"ncenter")) {
      buffer.AddVerticalJustification(kVerticalJustification_NCenter);
    } else if (Equals(str + 6, len - 6, L"nbottom")) {
      buffer.AddVerticalJustification(kVerticalJustification_NBottom);
    }
  } else if (Equals(str, len, L"push")) {
    buffer.AddPushState();
  } else if (Equals(str, len, L"pop")) {
    buffer.AddPopState();
  } else {
    HandleUserTag(buffer, str, len);
  }
}

bool CTextParser::BeginsWith(const wchar_t* str, int len, const wchar_t* prefix) {
  int i = 0;
  for (; prefix[i] && i < len; ++i) {
    if (str[i] != prefix[i]) {
      return false;
    }
  }
  return prefix[i] == L'\0';
}

bool CTextParser::Equals(const wchar_t* str, int len, const wchar_t* other) {
  int i = 0;
  for (; other[i] && i < len; ++i) {
    if (str[i] != other[i]) {
      return false;
    }
  }
  return other[i] == L'\0';
}

int CTextParser::ParseInt(const wchar_t* str, int len, bool allowSign) {
  bool negative = false;
  int pos = 0;
  if (allowSign && len > 0 && str[0] == L'-') {
    negative = true;
    pos = 1;
  }

  int val = 0;
  while (len > pos) {
    val *= 10;
    wchar_t ch = str[pos];
    val += ch - L'0';
    ++pos;
  }
  return negative ? -val : val;
}

int CTextParser::FromHex(wchar_t c) {
  if (c >= L'0' && c <= L'9') {
    return c - L'0';
  }
  if (c >= L'A' && c <= L'F') {
    return c - L'A' + 10;
  }
  if (c >= L'a' && c <= L'f') {
    return c - L'a' + 10;
  }
  return 0;
}

int CTextParser::GetColorValue(const wchar_t* str) {
  return FromHex(str[1]) + (FromHex(str[0]) << 4);
}

CTextColor CTextParser::ParseColor(const wchar_t* str, int len) {
  const int r = GetColorValue(str + 1);
  const int g = GetColorValue(str + 3);
  const int b = GetColorValue(str + 5);
  const int a = len == 9 ? GetColorValue(str + 7) : 255;
  return CTextColor(r, g, b, a);
}
