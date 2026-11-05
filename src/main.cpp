#include "speech_common.h"
#include <shellapi.h>

const wchar_t* commands[]={L"换行",L"逗号",L"句号",L"保存笔记",L"停止识别"};
class Recognition {
 Com<ISpRecognizer> engine;Com<ISpRecoContext> context;Com<ISpRecoGrammar> grammar;
 Com<ISpStream> input;Com<ISpAudio> microphone;
public:
 Tokens engines;bool active=false,ended=false,commandMode=false;std::vector<std::wstring> results;
 Recognition(){engines.load(SPCAT_RECOGNIZERS);}
 void stop(){if(engine.get())engine->SetRecoState(SPRST_INACTIVE_WITH_PURGE);grammar.reset();context.reset();engine.reset();if(input.get())input->Close();input.reset();microphone.reset();active=false;ended=false;}
 ~Recognition(){stop();}
 void start(int chosen,bool cmd,const std::wstring&file=L""){
  stop();ended=false;results.clear();commandMode=cmd;if(engines.list.empty())throw std::runtime_error("No desktop SAPI recognition engine installed");
  check(CoCreateInstance(CLSID_SpInprocRecognizer,nullptr,CLSCTX_ALL,IID_ISpRecognizer,(void**)engine.put()),"Create recognizer");
  check(engine->SetRecognizer(engines.list.at(chosen)),"Select recognition engine");
  check(engine->SetRecoState(SPRST_INACTIVE),"Prepare recognizer");
  if(file.empty()){check(CoCreateInstance(CLSID_SpMMAudioIn,nullptr,CLSCTX_ALL,IID_ISpAudio,(void**)microphone.put()),"Open default microphone");check(engine->SetInput(microphone.get(),TRUE),"Set microphone input");}
  else{check(CoCreateInstance(CLSID_SpStream,nullptr,CLSCTX_ALL,IID_ISpStream,(void**)input.put()),"Create input stream");check(input->BindToFile(file.c_str(),SPFM_OPEN_READONLY,nullptr,nullptr,0),"Open PCM WAV input");check(engine->SetInput(input.get(),TRUE),"Set WAV input");}
  check(engine->CreateRecoContext(context.put()),"Create recognition context");
  ULONGLONG events=SPFEI(SPEI_RECOGNITION)|SPFEI(SPEI_FALSE_RECOGNITION)|SPFEI(SPEI_END_SR_STREAM);
  check(context->SetInterest(events,events),"Subscribe recognition events");
  check(context->CreateGrammar(1,grammar.put()),"Create grammar");
  if(cmd){SPSTATEHANDLE state=nullptr;check(grammar->GetRule(L"commands",1,SPRAF_TopLevel|SPRAF_Active,TRUE,&state),"Create command rule");for(auto word:commands)check(grammar->AddWordTransition(state,nullptr,word,L" ",SPWT_LEXICAL,1.0f,nullptr),"Add command word");check(grammar->Commit(0),"Commit command grammar");check(grammar->SetRuleState(nullptr,nullptr,SPRS_ACTIVE),"Activate commands");}
  else{check(grammar->LoadDictation(nullptr,SPLO_STATIC),"Load dictation model");check(grammar->SetDictationState(SPRS_ACTIVE),"Activate dictation");}
  check(engine->SetRecoState(SPRST_ACTIVE_ALWAYS),"Start recognition");active=true;
 }
 struct Result{std::wstring words;bool rejected;int confidence;};
 std::vector<Result> poll(){std::vector<Result>out;if(!context.get())return out;SPEVENT event;ULONG fetched=0;while(context->GetEvents(1,&event,&fetched)==S_OK&&fetched){if(event.eEventId==SPEI_RECOGNITION){auto r=(ISpRecoResult*)event.lParam;wchar_t*words=nullptr;HRESULT hr=r->GetText(SP_GETWHOLEPHRASE,SP_GETWHOLEPHRASE,TRUE,&words,nullptr);if(SUCCEEDED(hr)){std::wstring s=words?words:L"";results.push_back(s);int confidence=0;SPPHRASE*phrase=nullptr;if(SUCCEEDED(r->GetPhrase(&phrase))){confidence=phrase->Rule.Confidence;CoTaskMemFree(phrase);}out.push_back({s,false,confidence});}CoTaskMemFree(words);}else if(event.eEventId==SPEI_FALSE_RECOGNITION)out.push_back({L"未能识别，请清晰重说",true,0});else if(event.eEventId==SPEI_END_SR_STREAM){ended=true;active=false;}freeEvent(event);}return out;}
 int recognizeFile(const std::wstring&file,bool cmd,const std::wstring&output){int chosen=engines.language(L"804");if(chosen<0)chosen=0;start(chosen,cmd,file);DWORD startTime=GetTickCount();std::wstring log=L"测试类型：合成音频文件输入（非真人麦克风测试）\r\n输入："+file+L"\r\n引擎："+engines.names[chosen]+L"\r\n模式："+(cmd?std::wstring(L"命令"):std::wstring(L"听写"))+L"\r\n";while(!ended&&GetTickCount()-startTime<45000){for(auto&r:poll())log+=(r.rejected?L"拒识：":L"识别：")+r.words+L"\r\n";MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(20);}bool timeout=!ended;bool success=!results.empty()&&(!cmd||std::find(results.begin(),results.end(),L"换行")!=results.end());log+=timeout?L"状态：TIMEOUT\r\n":(success?L"状态：PASS\r\n":L"状态：NO_MATCH\r\n");writeUtf8(output,log);stop();return success&&!timeout?0:2;}
};

class Notes {
 Recognition rec;HWND win,edit,log,status,combo,mode,startB,stopB,fileB,saveB,clearB;bool fileInput=false;
 void save(){auto p=fileDialog(win,true,L"UTF-8 文本\0*.txt\0",L"txt");if(!p.empty()){writeUtf8(p,text(edit));SetWindowTextW(status,(L"已保存："+p).c_str());}}
 void stop(){rec.stop();EnableWindow(combo,TRUE);EnableWindow(mode,TRUE);EnableWindow(startB,TRUE);EnableWindow(fileB,TRUE);EnableWindow(stopB,FALSE);}
 void start(const std::wstring&path){int sel=(int)SendMessageW(combo,CB_GETCURSEL,0,0);bool cmd=SendMessageW(mode,CB_GETCURSEL,0,0)==1;try{rec.start(sel,cmd,path);}catch(...){stop();throw;}fileInput=!path.empty();EnableWindow(combo,FALSE);EnableWindow(mode,FALSE);EnableWindow(startB,FALSE);EnableWindow(fileB,FALSE);EnableWindow(stopB,TRUE);SetWindowTextW(status,fileInput?L"正在识别 WAV 音频文件…":L"正在使用默认麦克风，请说话；点击“停止识别”结束。");append(log,L"开始："+(fileInput?path:std::wstring(L"默认麦克风"))+(cmd?L" [命令]\r\n":L" [听写]\r\n"));}
public:
 void create(HWND h){win=h;auto title=control(h,L"STATIC",L"实验2.2  语音识别记事本 · Microsoft SAPI",0,1);place(title,16,16,650,26);startB=control(h,L"BUTTON",L"开始麦克风",0,10);stopB=control(h,L"BUTTON",L"停止识别",0,11);fileB=control(h,L"BUTTON",L"识别 WAV",0,12);saveB=control(h,L"BUTTON",L"保存笔记",0,13);clearB=control(h,L"BUTTON",L"清空笔记",0,14);combo=control(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,15);for(auto&n:rec.engines.names)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)n.c_str());int zh=rec.engines.language(L"804");SendMessageW(combo,CB_SETCURSEL,zh>=0?zh:0,0);mode=control(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST,16);SendMessageW(mode,CB_ADDSTRING,0,(LPARAM)L"听写模式：说话转成文本");SendMessageW(mode,CB_ADDSTRING,0,(LPARAM)L"命令模式：语音操作笔记");SendMessageW(mode,CB_SETCURSEL,0,0);auto hint=control(h,L"STATIC",L"命令：换行 / 逗号 / 句号 / 保存笔记 / 停止识别；可先听写，再停止并切换命令模式。",0,17);place(hint,16,142,850,28);edit=control(h,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,18);SendMessageW(edit,EM_SETLIMITTEXT,1000000,0);log=control(h,L"EDIT",L"识别记录（保留引擎返回的原始结果）\r\n",ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,19);SendMessageW(log,EM_SETLIMITTEXT,1000000,0);status=control(h,L"STATIC",L"就绪 · WAV 输入建议使用 16 位单声道 PCM；语言必须与所选识别引擎匹配。",0,20);EnableWindow(stopB,FALSE);if(rec.engines.list.empty()){EnableWindow(startB,FALSE);EnableWindow(fileB,FALSE);SetWindowTextW(status,L"未检测到桌面 SAPI 识别引擎，请安装对应语言的 Windows 语音识别组件。");}SetTimer(h,1,100,nullptr);}
 void layout(int w,int h){place(startB,16,52,115,32);place(stopB,139,52,100,32);place(fileB,247,52,100,32);place(saveB,355,52,100,32);place(clearB,463,52,100,32);place(combo,16,100,(w-40)/2,180);place(mode,24+(w-40)/2,100,(w-40)/2,180);place(edit,16,176,w-32,h-370);place(log,16,h-180,w-32,112);place(status,16,h-54,w-32,42);}
 void command(int id){if(id==10)start(L"");if(id==11){stop();SetWindowTextW(status,L"已停止识别");}if(id==12){auto p=fileDialog(win,false,L"PCM WAV 音频\0*.wav\0",L"wav");if(!p.empty())start(p);}if(id==13)save();if(id==14){if(text(edit).empty()||MessageBoxW(win,L"确定清空笔记内容？",L"清空笔记",MB_YESNO|MB_ICONQUESTION)==IDYES)SetWindowTextW(edit,L"");}}
 void timer(){bool stopCommand=false;for(auto&r:rec.poll()){if(r.rejected){append(log,L"拒识："+r.words+L"\r\n");continue;}append(log,L"识别："+r.words+L" [置信级别 "+std::to_wstring(r.confidence)+L"]\r\n");if(!rec.commandMode)append(edit,r.words+L"\r\n");else if(r.words==L"换行")append(edit,L"\r\n");else if(r.words==L"逗号")append(edit,L"，");else if(r.words==L"句号")append(edit,L"。");else if(r.words==L"保存笔记")PostMessageW(win,WM_COMMAND,13,0);else if(r.words==L"停止识别")stopCommand=true;}if(stopCommand||rec.ended){stop();SetWindowTextW(status,stopCommand?L"已执行语音命令：停止识别":L"音频文件识别结束，请查看笔记与识别记录。");}}
};
Notes* app=nullptr;
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){try{if(m==WM_CREATE){app->create(h);return 0;}if(m==WM_SIZE){app->layout(LOWORD(l),HIWORD(l));return 0;}if(m==WM_GETMINMAXINFO){((MINMAXINFO*)l)->ptMinTrackSize={820,580};return 0;}if(m==WM_COMMAND){if(HIWORD(w)==BN_CLICKED)app->command(LOWORD(w));return 0;}if(m==WM_TIMER){app->timer();return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}}catch(const std::exception&e){error(h,e);}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(hr))return 1;int code=0;bool cli=false;std::wstring errorPath;try{int n;LPWSTR*a=CommandLineToArgvW(GetCommandLineW(),&n);if(n>=3&&std::wstring(a[1])==L"--recognize"){std::wstring file=a[2],out=L"recognition.txt";bool cmd=false;for(int i=3;i<n;++i){if(std::wstring(a[i])==L"--commands")cmd=true;else if(std::wstring(a[i])==L"--out"&&i+1<n)out=a[++i];}cli=true;errorPath=out;LocalFree(a);Recognition r;code=r.recognizeFile(file,cmd,out);}else{LocalFree(a);Notes notes;app=&notes;WNDCLASSW wc={};wc.hInstance=instance;wc.lpszClassName=L"SpeechNotesWindow";wc.lpfnWndProc=proc;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);HWND h=CreateWindowW(wc.lpszClassName,L"语音识别记事本 — 实验2.2",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,960,720,nullptr,nullptr,instance,nullptr);if(!h)throw std::runtime_error("Cannot create window");ShowWindow(h,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}}}catch(const std::exception&e){if(cli){try{writeUtf8(errorPath,L"ERROR: "+wide(e.what()));}catch(...){}}else error(nullptr,e);code=1;}CoUninitialize();return code;}





