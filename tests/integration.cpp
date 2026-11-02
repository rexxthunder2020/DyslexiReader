#define wWinMain ReaderEntry
#include "../src/main.cpp"
#undef wWinMain
#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fstream>
#include <stdexcept>
std::ofstream logFile;
void Check(bool good,const char* description) {
    logFile << (good?"PASS ":"FAIL ") << description << std::endl;
    if(!good) throw std::runtime_error(description);
}
struct Writer : FPDF_FILEWRITE {
    std::ofstream out;
    Writer():out("build/sample.pdf",std::ios::binary) {
        version=1;
        WriteBlock=[](FPDF_FILEWRITE* self,const void* data,unsigned long size)->int {
            auto w=static_cast<Writer*>(self); w->out.write((const char*)data,size); return w->out.good();
        };
    }
};
void Pump() {
    MSG m; while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    logFile.open("build/test-results.txt");
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); FPDF_InitLibrary();
    int result=0;
    try {
        auto sample=FPDF_CreateNewDocument();
        for(int p=0;p<2;++p) {
            auto pg=FPDFPage_New(sample,p,612,792);
            auto obj=FPDFPageObj_NewTextObj(sample,"Helvetica",24);
            const wchar_t* words=p?L"Second page. Reading has reached the end.":L"Hello reader. Select these words to hear them aloud.";
            Check(FPDFText_SetText(obj,(FPDF_WIDESTRING)words)!=0,"set sample PDF text");
            FPDFPageObj_Transform(obj,1,0,0,1,50,650);
            Check(FPDFPage_InsertObject(pg,obj)!=0,"insert sample text");
            Check(FPDFPage_GenerateContent(pg)!=0,"generate sample page");
            FPDF_ClosePage(pg);
        }
        { Writer writer; Check(FPDF_SaveAsCopy(sample,&writer,0)!=0,"save sample PDF"); }
        FPDF_CloseDocument(sample);
        WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"ReaderTest";
        RegisterClassW(&wc);
        win=CreateWindowW(wc.lpszClassName,L"test",WS_OVERLAPPEDWINDOW|WS_VSCROLL,0,0,1100,850,nullptr,nullptr,instance,nullptr);
        Check(win!=nullptr,"create native window");
        Load(L"build/sample.pdf");
        Check(doc && pages.size()==2 && count==2,"open generated PDF with two pages");
        Layout();
        RenderPage(0);
        Check(pages[0].bitmap && FPDFBitmap_GetWidth(pages[0].bitmap)>0,"render PDF bitmap");
        auto box=CharRect(0,0);
        auto hit=Hit((box.left+box.right)/2,(box.top+box.bottom)/2);
        Check(hit.page==0 && hit.ch==0,"map visible character to selection index");
        Check(documentHeight>Viewport().bottom-Viewport().top,"continuous document exceeds viewport");
        int oldScroll=scrollY;SendMessageW(win,WM_MOUSEWHEEL,MAKEWPARAM(0,(WORD)-WHEEL_DELTA),0);
        Check(scrollY>oldScroll && scrollY<pages[1].top,"mouse wheel scrolls within a page");
        ScrollTo(INT_MAX);
        Check(scrollY==documentHeight-(Viewport().bottom-Viewport().top),"scroll clamps at document end");
        SendMessageW(win,WM_VSCROLL,SB_TOP,0);Check(scrollY==0,"scrollbar returns to document start");
        SetZoom(.3);ScrollTo(0);
        RECT second=PaperRect(1),view=Viewport();
        Check(second.top>=view.top && second.bottom<=view.bottom,"zoom out displays two complete pages together");
        box=CharRect(1,0);hit=Hit((box.left+box.right)/2,(box.top+box.bottom)/2);
        Check(hit.page==1 && hit.ch==0,"hit testing on second simultaneously visible page");
        anchorPage=0;anchor=0;caretPage=1;caret=5;
        Check(Selected(0,0) && Selected(1,5) && !Selected(1,6),"selection extends across visible pages");
        // Verify that word changes invalidate a small region and reuse page pixels.
        RenderPage(0);auto cached=pages[0].bitmap;
        ValidateRect(win,nullptr);wordPage=0;word=0;wordEnd=5;InvalidateWord();
        RECT dirty=WordBounds();Check(!IsRectEmpty(&dirty) && dirty.bottom-dirty.top<100,"word update invalidates only highlight area");
        RECT client;GetClientRect(win,&client);HDC screen=GetDC(win),memory=CreateCompatibleDC(screen);
        HBITMAP frame=CreateCompatibleBitmap(screen,client.right,client.bottom),old=(HBITMAP)SelectObject(memory,frame);
        word=wordEnd=wordPage=-1;DrawDocument(memory,client);box=CharRect(0,0);
        COLORREF before=GetPixel(memory,(box.left+box.right)/2,(box.top+box.bottom)/2);
        wordPage=0;word=0;wordEnd=5;DrawDocument(memory,client);
        COLORREF during=GetPixel(memory,(box.left+box.right)/2,(box.top+box.bottom)/2);
        word=wordEnd=wordPage=-1;DrawDocument(memory,client);
        Check(before!=during && before==GetPixel(memory,(box.left+box.right)/2,(box.top+box.bottom)/2),"highlight paints and restores cached document pixels");
        Check(pages[0].bitmap==cached,"word changes reuse rendered page cache");
        SelectObject(memory,old);DeleteObject(frame);DeleteDC(memory);ReleaseDC(win,screen);
        ClearSelection();SetZoom(1);ScrollTo(0);
        speech.clear();positions.clear();AddPage(0,0,4);
        Check(speech==L"Hello\n" && positions.size()==6,"selected text extraction and position mapping");
        Check(SUCCEEDED(CoCreateInstance(CLSID_SpVoice,nullptr,CLSCTX_ALL,IID_ISpVoice,(void**)&voice)),"create installed Windows speech voice");
        voice->SetVolume(0); voice->SetRate(7);
        Check(SUCCEEDED(voice->SetNotifyWindowMessage(win,WM_APP+1,0,0)),"connect speech events");
        ULONGLONG events=SPFEI(SPEI_WORD_BOUNDARY)|SPFEI(SPEI_END_INPUT_STREAM);
        Check(SUCCEEDED(voice->SetInterest(events,events)),"subscribe word and completion events");
        anchorPage=caretPage=0;anchor=0;caret=4;Read();
        Check(speaking && speech==L"Hello\n","play selected text only");
        SendMessageW(win,WM_COMMAND,Pause,0);Check(paused,"pause speech");
        Read();Check(!paused && speaking,"resume speech");
        Halt();Check(!speaking && !paused && word==-1,"stop and clear speech state");
        anchorPage=1;anchor=5;caretPage=0;caret=0;Read();
        Check(speaking && speech.find(L"Hello")!=std::wstring::npos && speech.find(L"Second")!=std::wstring::npos &&
            speech.find(L"reached")==std::wstring::npos,"reverse cross-page selection reads only selected range");
        Halt();ClearSelection();Read();
        Check(speaking && speech.find(L"Second page")!=std::wstring::npos,"play full document without selection");
        bool highlight=false,secondPage=false;
        ULONGLONG deadline=GetTickCount64()+20000;
        while(speaking && GetTickCount64()<deadline) {
            Pump(); highlight=highlight || word>=0;secondPage=secondPage || current==1; Sleep(5);
        }
        Check(!speaking,"speech completion event");
        Check(highlight,"word boundary events produce highlights");
        Check(secondPage,"speech automatically follows second page");
        Check(scrollY>0,"speech scrolls to keep spoken page visible");
        Go(0);Check(current==0,"previous page navigation");
        Go(-1);Check(current==0,"navigation bounds");
        DestroyWindow(win);
        voice->Release();voice=nullptr;
    } catch(...) { result=1; if(win && IsWindow(win)) DestroyWindow(win); if(voice) voice->Release(); }
    FPDF_DestroyLibrary();CoUninitialize();return result;
}
