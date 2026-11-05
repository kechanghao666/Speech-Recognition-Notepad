#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <sapi.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cwctype>

template<class T> class Com {
 T* p=nullptr;
public:
 Com()=default; ~Com(){reset();} Com(const Com&)=delete; Com& operator=(const Com&)=delete;
 T* operator->() const{return p;} T* get() const{return p;}
 T** put(){reset();return &p;} void reset(){if(p){p->Release();p=nullptr;}}
};
inline void check(HRESULT hr,const char* step){if(FAILED(hr)){std::ostringstream s;s<<step<<" (HRESULT 0x"<<std::hex<<(unsigned long)hr<<")";throw std::runtime_error(s.str());}}
inline std::wstring wide(const std::string&s,UINT cp=CP_UTF8){if(s.empty())return L"";int n=MultiByteToWideChar(cp,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,0);MultiByteToWideChar(cp,0,s.data(),(int)s.size(),&w[0],n);return w;}
inline std::string utf8(const std::wstring&w){if(w.empty())return "";int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),&s[0],n,nullptr,nullptr);return s;}
inline std::wstring text(HWND h){int n=GetWindowTextLengthW(h);std::vector<wchar_t>b(n+1);GetWindowTextW(h,b.data(),n+1);return b.data();}
inline void writeUtf8(const std::wstring&path,const std::wstring&w){std::string s="\xef\xbb\xbf"+utf8(w);HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create output file");DWORD n=0;BOOL ok=WriteFile(f,s.data(),(DWORD)s.size(),&n,nullptr);CloseHandle(f);if(!ok||n!=s.size())throw std::runtime_error("Cannot write output file");}
inline std::wstring readText(const std::wstring&path){HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open input file");LARGE_INTEGER sz;GetFileSizeEx(f,&sz);if(sz.QuadPart>4*1024*1024){CloseHandle(f);throw std::runtime_error("Text file exceeds 4 MB");}std::string s((size_t)sz.QuadPart,0);DWORD n=0;BOOL ok=ReadFile(f,s.data(),(DWORD)s.size(),&n,nullptr);CloseHandle(f);if(!ok)throw std::runtime_error("Cannot read input file");if(s.size()>=2&&((unsigned char)s[0]==255&&(unsigned char)s[1]==254)){if((s.size()-2)%2)throw std::runtime_error("Invalid UTF-16LE file");return std::wstring((const wchar_t*)(s.data()+2),(s.size()-2)/2);}if(s.size()>=2&&(unsigned char)s[0]==254&&(unsigned char)s[1]==255){std::wstring w;for(size_t i=2;i+1<s.size();i+=2)w+=(wchar_t)(((unsigned char)s[i]<<8)|(unsigned char)s[i+1]);return w;}if(s.substr(0,3)=="\xef\xbb\xbf")s.erase(0,3);if(s.empty())return L"";if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0))return wide(s);return wide(s,936);}
inline std::wstring fileDialog(HWND owner,bool save,const wchar_t*filter,const wchar_t*ext){wchar_t path[32768]={0};OPENFILENAMEW o={sizeof(o)};o.hwndOwner=owner;o.lpstrFilter=filter;o.lpstrFile=path;o.nMaxFile=32768;o.lpstrDefExt=ext;o.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);return (save?GetSaveFileNameW(&o):GetOpenFileNameW(&o))?path:L"";}
struct Tokens {
 std::vector<ISpObjectToken*> list;std::vector<std::wstring> names;
 ~Tokens(){for(auto p:list)p->Release();}
 void load(const wchar_t*category){Com<ISpObjectTokenCategory> c;check(CoCreateInstance(CLSID_SpObjectTokenCategory,nullptr,CLSCTX_ALL,IID_ISpObjectTokenCategory,(void**)c.put()),"Token category");check(c->SetId(category,FALSE),"Token category ID");Com<IEnumSpObjectTokens> e;check(c->EnumTokens(nullptr,nullptr,e.put()),"Enumerate tokens");ISpObjectToken*t=nullptr;ULONG count;while(e->Next(1,&t,&count)==S_OK){wchar_t*n=nullptr;HRESULT hr=t->GetStringValue(nullptr,&n);list.push_back(t);names.push_back(SUCCEEDED(hr)&&n?n:L"Unnamed engine");CoTaskMemFree(n);}}
 int language(const wchar_t*lang)const{for(size_t i=0;i<list.size();++i){Com<ISpDataKey> a;wchar_t*v=nullptr;if(SUCCEEDED(list[i]->OpenKey(L"Attributes",a.put()))&&SUCCEEDED(a->GetStringValue(L"Language",&v))){std::wstring x=v;CoTaskMemFree(v);if(x.find(lang)!=std::wstring::npos)return (int)i;}}return -1;}
};
inline HWND control(HWND parent,const wchar_t*cls,const wchar_t*caption,DWORD style,int id){HWND h=CreateWindowExW((wcscmp(cls,L"EDIT")==0)?WS_EX_CLIENTEDGE:0,cls,caption,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);static HFONT font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);return h;}
inline void place(HWND h,int x,int y,int w,int height){MoveWindow(h,x,y,std::max(w,1),height,TRUE);}
inline void append(HWND h,const std::wstring&s){int end=GetWindowTextLengthW(h);SendMessageW(h,EM_SETSEL,end,end);SendMessageW(h,EM_REPLACESEL,FALSE,(LPARAM)s.c_str());}
inline void error(HWND h,const std::exception&e){std::wstring s=L"操作未完成："+wide(e.what())+L"\r\n请检查语音引擎、音频设备、文件格式和保存路径。";MessageBoxW(h,s.c_str(),L"语音实验",MB_OK|MB_ICONERROR);}
inline WAVEFORMATEX pcm(){WAVEFORMATEX w={};w.wFormatTag=WAVE_FORMAT_PCM;w.nChannels=1;w.nSamplesPerSec=22050;w.wBitsPerSample=16;w.nBlockAlign=2;w.nAvgBytesPerSec=44100;return w;}
inline void freeEvent(SPEVENT&e){if(e.elParamType==SPET_LPARAM_IS_OBJECT||e.elParamType==SPET_LPARAM_IS_TOKEN){if(e.lParam)((IUnknown*)e.lParam)->Release();}else if(e.elParamType==SPET_LPARAM_IS_POINTER||e.elParamType==SPET_LPARAM_IS_STRING)CoTaskMemFree((void*)e.lParam);}

// MinGW libsapi omits this format GUID; use an internal constant for portability.
inline const GUID WaveFormatGuid={0xc31adbae,0x527f,0x4ff5,{0xa2,0x30,0xf6,0x2b,0xb6,0x1f,0xf7,0x0c}};



