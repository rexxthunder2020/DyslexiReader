#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sapi.h>
#include <fpdfview.h>
#include <fpdf_text.h>
#include <algorithm>
#include <climits>
#include <string>
#include <vector>
#include <cmath>

struct Pos { int page, ch; };
struct PageView {
    double width=612,height=792;
    int top=0,pixelWidth=0,pixelHeight=0;
    FPDF_PAGE page=nullptr;
    FPDF_TEXTPAGE text=nullptr;
    FPDF_BITMAP bitmap=nullptr;
};
HWND win; ISpVoice* voice=nullptr;
FPDF_DOCUMENT doc=nullptr;
std::vector<PageView> pages;
int current=0,count=0,anchor=-1,caret=-1,word=-1,wordEnd=-1;
int anchorPage=-1,caretPage=-1,wordPage=-1;
int scrollY=0,documentHeight=0,layoutWidth=0,wheelRemainder=0;
double zoom=1.0;
bool dragging=false,speaking=false,paused=false;
std::wstring speech,filename; std::vector<Pos> positions; ULONG stream=0;
enum { Open=100,Play,Pause,Stop,Previous,Next,Clear,Register,Slower,Faster,ZoomIn,ZoomOut };
constexpr int ViewTop=48,BottomBar=28,PageGap=20;
RECT Viewport() { RECT r;GetClientRect(win,&r);r.top=ViewTop;r.bottom=std::max(r.top,r.bottom-BottomBar);return r; }
void Layout();
void ScrollTo(int y);
RECT CharRect(int p,int i);
void ClearSelection() { anchor=caret=anchorPage=caretPage=-1; }
RECT WordBounds() {
    RECT bounds{};
    if(wordPage<0 || wordPage>=count || word<0) return bounds;
    RECT viewport=Viewport();
    for(int i=word;i<wordEnd;++i) {
        RECT box=CharRect(wordPage,i),clipped;
        InflateRect(&box,2,2);
        if(IntersectRect(&clipped,&box,&viewport)) UnionRect(&bounds,&bounds,&clipped);
    }
    return bounds;
}
void InvalidateWord() {
    RECT bounds=WordBounds();
    if(!IsRectEmpty(&bounds)) InvalidateRect(win,&bounds,FALSE);
}
void Error(const wchar_t* s) { MessageBoxW(win,s,L"DyslexiReader",MB_OK|MB_ICONINFORMATION); }
void Refresh() { InvalidateRect(win,nullptr,FALSE); }
void Title() {
    std::wstring s=L"DyslexiReader";
    if(doc) s+=L" — "+filename+L" — Page "+std::to_wstring(current+1)+L" / "+std::to_wstring(count);
    if(paused) s+=L" — Paused"; else if(speaking) s+=L" — Reading";
    SetWindowTextW(win,s.c_str());
}
void Halt() {
    if(voice) {
        if(paused) voice->Resume();
        voice->Speak(nullptr,SPF_PURGEBEFORESPEAK,nullptr);
        SPEVENT e{}; ULONG n;
        while(voice->GetEvents(1,&e,&n)==S_OK && n) {}
    }
    paused=speaking=false; word=wordEnd=wordPage=-1; Title(); Refresh();
}
void Release(PageView& p) {
    if(p.bitmap) FPDFBitmap_Destroy(p.bitmap); p.bitmap=nullptr;
    if(p.text) FPDFText_ClosePage(p.text); p.text=nullptr;
    if(p.page) FPDF_ClosePage(p.page); p.page=nullptr;
}
void ReleasePage() {
    for(auto& p:pages) Release(p);
    pages.clear(); layoutWidth=0;documentHeight=scrollY=0;
}
bool EnsurePage(int n) {
    auto& p=pages[n];
    if(!p.page) p.page=FPDF_LoadPage(doc,n);
    if(p.page && !p.text) p.text=FPDFText_LoadPage(p.page);
    return p.page!=nullptr;
}
void Go(int n) {
    if(!doc || n<0 || n>=count) return;
    Layout(); ScrollTo(pages[n].top); current=n;Title();
}
void Load(const wchar_t* path) {
    int len=WideCharToMultiByte(CP_UTF8,0,path,-1,nullptr,0,nullptr,nullptr);
    std::string utf(len,'\0');
    WideCharToMultiByte(CP_UTF8,0,path,-1,utf.data(),len,nullptr,nullptr);
    auto next=FPDF_LoadDocument(utf.c_str(),nullptr);
    if(!next) { Error(L"Cannot load this PDF. It may be inaccessible, damaged or password protected. Save an unlocked copy and try again."); return; }
    if(FPDF_GetPageCount(next)<1) { FPDF_CloseDocument(next); Error(L"This PDF has no pages."); return; }
    Halt(); ReleasePage(); if(doc) FPDF_CloseDocument(doc);
    doc=next; filename=path; count=FPDF_GetPageCount(doc);current=0;
    pages.resize(count);ClearSelection();wheelRemainder=0;
    for(int i=0;i<count;++i) {
        double width=612,height=792;
        if(FPDF_GetPageSizeByIndex(doc,i,&width,&height) && width>0 && height>0) {
            pages[i].width=width;pages[i].height=height;
        }
    }
    Layout();Title();Refresh();
}
void Choose() {
    wchar_t path[32768]{}; OPENFILENAMEW o{sizeof(o)};
    o.hwndOwner=win; o.lpstrFilter=L"PDF files\0*.pdf\0All files\0*.*\0";
    o.lpstrFile=path; o.nMaxFile=32768; o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&o)) Load(path);
}
void AddPage(int p,int first,int last) {
    auto pg=FPDF_LoadPage(doc,p); if(!pg) return;
    auto tp=FPDFText_LoadPage(pg);
    if(tp) {
        int n=FPDFText_CountChars(tp); last=std::min(last,n-1);
        for(int i=first;i<=last;++i) {
            unsigned u=FPDFText_GetUnicode(tp,i);
            if(!u || u>0x10ffff) continue;
            if(u<=0xffff) { speech.push_back((wchar_t)u); positions.push_back({p,i}); }
            else {
                u-=0x10000;
                speech.push_back((wchar_t)(0xd800+(u>>10))); positions.push_back({p,i});
                speech.push_back((wchar_t)(0xdc00+(u&1023))); positions.push_back({p,i});
            }
        }
        FPDFText_ClosePage(tp);
    }
    FPDF_ClosePage(pg); speech+=L"\n"; positions.push_back({p,-1});
}
void Read() {
    if(!voice) { Error(L"Windows speech is unavailable. Install a Windows speech voice in Settings."); return; }
    if(paused) { if(SUCCEEDED(voice->Resume())) paused=false; Title(); return; }
    if(speaking || !doc) return;
    speech.clear(); positions.clear(); SetCursor(LoadCursor(nullptr,IDC_WAIT));
    if(anchor>=0 && caret>=0 && anchorPage>=0 && caretPage>=0) {
        Pos a{anchorPage,anchor},b{caretPage,caret};
        if(a.page>b.page || (a.page==b.page && a.ch>b.ch)) std::swap(a,b);
        for(int p=a.page;p<=b.page;++p) AddPage(p,p==a.page?a.ch:0,p==b.page?b.ch:INT_MAX);
    }
    else for(int p=0;p<count;++p) AddPage(p,0,INT_MAX);
    SetCursor(LoadCursor(nullptr,IDC_ARROW));
    if(speech.find_first_not_of(L" \r\n\t")==std::wstring::npos) {
        Error(L"No readable text was found. Image-only scanned PDFs need OCR first."); return;
    }
    if(FAILED(voice->Speak(speech.c_str(),SPF_ASYNC|SPF_IS_NOT_XML,&stream))) {
        Error(L"Speech playback could not start."); return;
    }
    speaking=true; Title();
}
void UpdateScrollBar() {
    RECT v=Viewport();
    SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL};
    si.nMin=0;si.nMax=std::max(0,documentHeight-1);si.nPage=v.bottom-v.top;si.nPos=scrollY;
    SetScrollInfo(win,SB_VERT,&si,TRUE);
}
void Layout() {
    if(!doc) { UpdateScrollBar();return; }
    RECT r;GetClientRect(win,&r);
    if(layoutWidth!=r.right) {
        // Preserve the page and relative location when resizing or zooming.
        int savedPage=0;
        for(int i=0;i<count;++i) if(pages[i].pixelHeight>0 && pages[i].top<=scrollY) savedPage=i;
        double fraction=pages[savedPage].pixelHeight>0?
            double(scrollY-pages[savedPage].top)/pages[savedPage].pixelHeight:0;
        layoutWidth=r.right;int top=PageGap;
        // At 100%, normal pages are 816 pixels wide (96 dpi). Smaller zoom
        // lets several complete pages share the viewport; wide pages fit it.
        double maxWidth=1;for(auto& p:pages) maxWidth=std::max(maxWidth,p.width);
        double scale=std::max(.01,std::min(96.0/72.0*zoom,(r.right-40)/maxWidth));
        for(auto& p:pages) {
            int width=std::max(1,(int)std::round(p.width*scale));
            int height=std::max(1,(int)std::round(p.height*scale));
            if(p.bitmap && (p.pixelWidth!=width || p.pixelHeight!=height)) {
                FPDFBitmap_Destroy(p.bitmap);p.bitmap=nullptr;
            }
            p.pixelWidth=width;p.pixelHeight=height;p.top=top;top+=height+PageGap;
        }
        documentHeight=top;scrollY=pages[savedPage].top+(int)(fraction*pages[savedPage].pixelHeight);
    }
    RECT v=Viewport();scrollY=std::clamp(scrollY,0,std::max<int>(0,documentHeight-(v.bottom-v.top)));
    UpdateScrollBar();
}
RECT PaperRect(int n) {
    RECT r;GetClientRect(win,&r);auto& p=pages[n];
    int left=(r.right-p.pixelWidth)/2,top=ViewTop+p.top-scrollY;
    return {left,top,left+p.pixelWidth,top+p.pixelHeight};
}
void ScrollTo(int y) {
    RECT v=Viewport();int next=std::clamp(y,0,std::max<int>(0,documentHeight-(v.bottom-v.top)));
    if(next==scrollY) return;
    scrollY=next;UpdateScrollBar();
    // The page containing the viewport center is the navigation reference.
    int mid=scrollY+(v.bottom-v.top)/2;
    for(int i=0;i<count;++i) if(pages[i].top<=mid) current=i;
    Title();InvalidateRect(win,&v,FALSE);
}
void SetZoom(double value) {
    zoom=std::clamp(value,.25,2.0);layoutWidth=0;Layout();Refresh();
}
RECT CharRect(int n,int i) {
    if(n<0 || n>=count || !EnsurePage(n) || !pages[n].text) return {};
    auto& p=pages[n];RECT paper=PaperRect(n);
    double l=0,r=0,b=0,t=0;FPDFText_GetCharBox(p.text,i,&l,&r,&b,&t);
    int x1=0,y1=0,x2=0,y2=0;
    FPDF_PageToDevice(p.page,paper.left,paper.top,p.pixelWidth,p.pixelHeight,0,l,t,&x1,&y1);
    FPDF_PageToDevice(p.page,paper.left,paper.top,p.pixelWidth,p.pixelHeight,0,r,b,&x2,&y2);
    return {std::min(x1,x2),std::min(y1,y2),std::max(x1,x2)+1,std::max(y1,y2)+1};
}
Pos Hit(int x,int y) {
    RECT v=Viewport();if(!PtInRect(&v,{x,y})) return {-1,-1};
    for(int n=0;n<count;++n) {
        RECT paper=PaperRect(n);
        if(!PtInRect(&paper,{x,y}) || !EnsurePage(n) || !pages[n].text) continue;
        auto& p=pages[n];double px=0,py=0;
        FPDF_DeviceToPage(p.page,paper.left,paper.top,p.pixelWidth,p.pixelHeight,0,x,y,&px,&py);
        int ch=FPDFText_GetCharIndexAtPos(p.text,px,py,12,12);
        return {n,ch};
    }
    return {-1,-1};
}
void FollowWord() {
    if(wordPage<0 || word<0) return;
    RECT r=CharRect(wordPage,word),v=Viewport();
    if(r.top<v.top+16 || r.bottom>v.bottom-16)
        ScrollTo(scrollY+r.top-v.top-std::min<int>(120,(v.bottom-v.top)/3));
    if(current!=wordPage) { current=wordPage;Title(); }
}
bool Selected(int p,int i) {
    if(anchor<0 || caret<0 || anchorPage<0 || caretPage<0) return false;
    Pos a{anchorPage,anchor},b{caretPage,caret};
    if(a.page>b.page || (a.page==b.page && a.ch>b.ch)) std::swap(a,b);
    return p>=a.page && p<=b.page && (p!=a.page || i>=a.ch) && (p!=b.page || i<=b.ch);
}
void RenderPage(int n) {
    auto& p=pages[n];if(p.bitmap || !EnsurePage(n)) return;
    p.bitmap=FPDFBitmap_Create(p.pixelWidth,p.pixelHeight,0);
    if(p.bitmap) {
        FPDFBitmap_FillRect(p.bitmap,0,0,p.pixelWidth,p.pixelHeight,0xffffffff);
        FPDF_RenderPageBitmap(p.bitmap,p.page,0,0,p.pixelWidth,p.pixelHeight,0,FPDF_ANNOT);
    }
}
void DrawDocument(HDC dc,const RECT& dirty) {
    FillRect(dc,&dirty,(HBRUSH)GetStockObject(DKGRAY_BRUSH));
    RECT v=Viewport(),clip;
    if(IntersectRect(&clip,&dirty,&v)) {
        int saved=SaveDC(dc);IntersectClipRect(dc,clip.left,clip.top,clip.right,clip.bottom);
        for(int n=0;n<count;++n) {
            RECT paper=PaperRect(n),visible;
            if(!IntersectRect(&visible,&paper,&v)) {
                // Keep resources bounded to visible pages and active text pages.
                if(n!=wordPage && n!=anchorPage && n!=caretPage) Release(pages[n]);
                continue;
            }
            if(!IntersectRect(&visible,&paper,&clip)) continue;
            RenderPage(n);auto& p=pages[n];
            if(p.bitmap) {
                BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),p.pixelWidth,-p.pixelHeight,1,32,BI_RGB};
                SetDIBitsToDevice(dc,paper.left,paper.top,p.pixelWidth,p.pixelHeight,0,0,0,p.pixelHeight,FPDFBitmap_GetBuffer(p.bitmap),&info,DIB_RGB_COLORS);
                if(p.text) for(int i=0,total=FPDFText_CountChars(p.text);i<total;++i) {
                    bool active=n==wordPage && word>=0 && i>=word && i<wordEnd;
                    if(!active && !Selected(n,i)) continue;
                    RECT box=CharRect(n,i),intersection;
                    if(!IntersectRect(&intersection,&box,&clip)) continue;
                    HBRUSH brush=CreateSolidBrush(active?RGB(80,80,255):RGB(255,150,60));
                    HBRUSH old=(HBRUSH)SelectObject(dc,brush);
                    PatBlt(dc,box.left,box.top,box.right-box.left,box.bottom-box.top,PATINVERT);
                    SelectObject(dc,old);DeleteObject(brush);
                }
            } else {
                FillRect(dc,&paper,(HBRUSH)GetStockObject(WHITE_BRUSH));
                DrawTextW(dc,L"This page could not be rendered.",-1,&paper,DT_CENTER|DT_SINGLELINE);
            }
        }
        RestoreDC(dc,saved);
    }
    RECT r;GetClientRect(win,&r);RECT hint{16,r.bottom-26,r.right-10,r.bottom};
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    std::wstring message=doc?L"Scroll to browse • Drag to select • Space plays/pauses • Ctrl+wheel zooms • "+std::to_wstring((int)std::round(zoom*100))+L"%":L"Open a PDF from File > Open, or drop a PDF here.";
    DrawTextW(dc,message.c_str(),-1,&hint,DT_SINGLELINE);
}
void Paint() {
    PAINTSTRUCT ps;HDC screen=BeginPaint(win,&ps);Layout();
    RECT r;GetClientRect(win,&r);
    // Compose the background, cached pages and overlays away from the screen.
    // Only the invalidated region is presented, in a single copy.
    HDC memory=CreateCompatibleDC(screen);
    HBITMAP frame=CreateCompatibleBitmap(screen,std::max(1L,r.right),std::max(1L,r.bottom));
    if(memory && frame) {
        auto old=SelectObject(memory,frame);
        IntersectClipRect(memory,ps.rcPaint.left,ps.rcPaint.top,ps.rcPaint.right,ps.rcPaint.bottom);
        DrawDocument(memory,ps.rcPaint);
        BitBlt(screen,ps.rcPaint.left,ps.rcPaint.top,ps.rcPaint.right-ps.rcPaint.left,ps.rcPaint.bottom-ps.rcPaint.top,memory,ps.rcPaint.left,ps.rcPaint.top,SRCCOPY);
        SelectObject(memory,old);
    } else DrawDocument(screen,ps.rcPaint);
    if(frame) DeleteObject(frame);if(memory) DeleteDC(memory);
    EndPaint(win,&ps);
}
bool SetReg(const std::wstring& key,const wchar_t* name,const std::wstring& value) {
    HKEY k;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,key.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&k,nullptr)!=ERROR_SUCCESS) return false;
    auto status=RegSetValueExW(k,name,0,REG_SZ,(const BYTE*)value.c_str(),(DWORD)((value.size()+1)*sizeof(wchar_t)));
    RegCloseKey(k); return status==ERROR_SUCCESS;
}
void Associate() {
    wchar_t exe[32768]; GetModuleFileNameW(nullptr,exe,32768);
    std::wstring cmd=L"\""+std::wstring(exe)+L"\" \"%1\"";
    bool ok=SetReg(L"Software\\Classes\\DyslexiReader.PDF",nullptr,L"PDF document")
        && SetReg(L"Software\\Classes\\DyslexiReader.PDF\\shell\\open\\command",nullptr,cmd)
        && SetReg(L"Software\\Classes\\Applications\\DyslexiReader.exe",L"FriendlyAppName",L"DyslexiReader")
        && SetReg(L"Software\\Classes\\Applications\\DyslexiReader.exe\\shell\\open\\command",nullptr,cmd)
        && SetReg(L"Software\\Classes\\Applications\\DyslexiReader.exe\\SupportedTypes",L".pdf",L"");
    HKEY k;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Classes\\.pdf\\OpenWithProgids",0,nullptr,0,KEY_WRITE,nullptr,&k,nullptr)==ERROR_SUCCESS) {
        ok=ok && RegSetValueExW(k,L"DyslexiReader.PDF",0,REG_NONE,nullptr,0)==ERROR_SUCCESS; RegCloseKey(k);
    } else ok=false;
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
    Error(ok?L"Registered for this user. Right-click a PDF > Open with > Choose another app > DyslexiReader, then choose Always. Keep this application folder in its current location.":L"Could not complete file association registration.");
}
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    switch(m) {
    case WM_CREATE: {
        win=h;
        const wchar_t* labels[]={L"Open PDF",L"Play",L"Pause",L"Stop",L"Previous",L"Next"};
        for(int i=0;i<6;++i) CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP,10+i*105,8,100,30,h,(HMENU)(INT_PTR)(Open+i),nullptr,nullptr);
        DragAcceptFiles(h,TRUE); break;
    }
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case Open:Choose();break; case Play:Read();break;
        case Pause:if(voice && speaking && !paused && SUCCEEDED(voice->Pause())) { paused=true; Title(); } break;
        case Stop:Halt();break; case Previous:Go(current-1);break; case Next:Go(current+1);break;
        case Clear:ClearSelection();Refresh();break; case Register:Associate();break;
        case ZoomIn:SetZoom(zoom+.1);break;case ZoomOut:SetZoom(zoom-.1);break;
        case Slower:case Faster:
            if(voice) { long rate=0;voice->GetRate(&rate);voice->SetRate(std::clamp(rate+(LOWORD(w)==Faster?1:-1),-10L,10L)); } break;
        }
        return 0;
    case WM_APP+1:
        if(voice) {
            SPEVENT e{}; ULONG fetched=0;
            while(voice->GetEvents(1,&e,&fetched)==S_OK && fetched) {
                if(speaking && e.ulStreamNum==stream) {
                    if(e.eEventId==SPEI_WORD_BOUNDARY) {
                        size_t start=(size_t)e.lParam,end=std::min(positions.size(),start+(size_t)e.wParam);
                        if(start<positions.size() && positions[start].ch>=0) {
                            InvalidateWord();
                            auto pos=positions[start];wordPage=pos.page;
                            word=pos.ch; wordEnd=word+1;
                            for(size_t j=start;j<end && positions[j].page==wordPage;++j) wordEnd=std::max(wordEnd,positions[j].ch+1);
                            FollowWord();InvalidateWord();
                        }
                    } else if(e.eEventId==SPEI_END_INPUT_STREAM) {
                        InvalidateWord();speaking=paused=false;word=wordEnd=wordPage=-1;Title();
                    }
                }
            }
        }
        return 0;
    case WM_KEYDOWN:
        if(w==VK_SPACE) SendMessageW(h,WM_COMMAND,speaking&&!paused?Pause:Play,0);
        else if(w==VK_ESCAPE) { ClearSelection();Refresh(); }
        else if(w==VK_NEXT) ScrollTo(scrollY+Viewport().bottom-Viewport().top-40);
        else if(w==VK_PRIOR) ScrollTo(scrollY-Viewport().bottom+Viewport().top+40);
        else if(w==VK_DOWN) ScrollTo(scrollY+48);else if(w==VK_UP) ScrollTo(scrollY-48);
        else if(w==VK_HOME) ScrollTo(0);else if(w==VK_END) ScrollTo(documentHeight);
        else if(w=='O' && GetKeyState(VK_CONTROL)<0) Choose();
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(h); { Pos hit=Hit(GET_X_LPARAM(l),GET_Y_LPARAM(l));
        anchor=caret=hit.ch;anchorPage=caretPage=hit.page; }
        dragging=anchor>=0; if(dragging) { SetCapture(h);SetTimer(h,1,40,nullptr); } Refresh();return 0;
    case WM_MOUSEMOVE:
        if(dragging) {
            Pos hit=Hit(GET_X_LPARAM(l),GET_Y_LPARAM(l));
            if(hit.ch>=0) { caret=hit.ch;caretPage=hit.page; } Refresh();
        } return 0;
    case WM_LBUTTONUP:if(dragging) { dragging=false;KillTimer(h,1);ReleaseCapture(); } return 0;
    case WM_CAPTURECHANGED:dragging=false;KillTimer(h,1);return 0;
    case WM_MOUSEWHEEL:
        wheelRemainder+=GET_WHEEL_DELTA_WPARAM(w);
        if(int ticks=wheelRemainder/WHEEL_DELTA) {
            wheelRemainder%=WHEEL_DELTA;
            if(GET_KEYSTATE_WPARAM(w)&MK_CONTROL) SetZoom(zoom+ticks*.1);
            else {
                UINT lines=3;SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&lines,0);
                ScrollTo(scrollY-ticks*(lines==WHEEL_PAGESCROLL?Viewport().bottom-Viewport().top-40:(int)lines*32));
            }
        }
        return 0;
    case WM_VSCROLL: {
        SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(h,SB_VERT,&si);
        int y=scrollY,step=Viewport().bottom-Viewport().top-40;
        switch(LOWORD(w)) {
        case SB_LINEUP:y-=48;break;case SB_LINEDOWN:y+=48;break;
        case SB_PAGEUP:y-=step;break;case SB_PAGEDOWN:y+=step;break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:y=si.nTrackPos;break;
        case SB_TOP:y=0;break;case SB_BOTTOM:y=documentHeight;break;
        }
        ScrollTo(y);return 0;
    }
    case WM_TIMER:
        if(w==1 && dragging) {
            POINT cursor;GetCursorPos(&cursor);ScreenToClient(h,&cursor);RECT v=Viewport();
            if(cursor.y<v.top+24) ScrollTo(scrollY-28);
            else if(cursor.y>v.bottom-24) ScrollTo(scrollY+28);
            Pos hit=Hit(cursor.x,std::clamp(cursor.y,v.top+1,v.bottom-1));
            if(hit.ch>=0 && (hit.ch!=caret || hit.page!=caretPage)) {
                caret=hit.ch;caretPage=hit.page;Refresh();
            }
        }
        return 0;
    case WM_DROPFILES: {
        wchar_t path[32768]; DragQueryFileW((HDROP)w,0,path,32768);
        DragFinish((HDROP)w);Load(path);return 0;
    }
    case WM_SIZE:Layout();Refresh();return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize={680,360};return 0;
    case WM_PAINT:Paint();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_DESTROY:
        Halt();ReleasePage();if(doc) FPDF_CloseDocument(doc);doc=nullptr;PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware(); CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); FPDF_InitLibrary();
    CoCreateInstance(CLSID_SpVoice,nullptr,CLSCTX_ALL,IID_ISpVoice,(void**)&voice);
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"DyslexiReaderWindow";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);RegisterClassW(&wc);
    HMENU menu=CreateMenu(),file=CreatePopupMenu(),read=CreatePopupMenu();
    AppendMenuW(file,MF_STRING,Open,L"&Open PDF...\tCtrl+O");
    AppendMenuW(file,MF_STRING,Register,L"Register PDF Open with option");
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)file,L"&File");
    const wchar_t* labels[]={L"&Play / Resume",L"&Pause",L"&Stop"};
    for(int i=0;i<3;++i) AppendMenuW(read,MF_STRING,Play+i,labels[i]);
    AppendMenuW(read,MF_STRING,Clear,L"Clear selection\tEsc");
    AppendMenuW(read,MF_STRING,Slower,L"Slower voice");AppendMenuW(read,MF_STRING,Faster,L"Faster voice");
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)read,L"&Read aloud");
    HMENU view=CreatePopupMenu();
    AppendMenuW(view,MF_STRING,ZoomIn,L"Zoom in");AppendMenuW(view,MF_STRING,ZoomOut,L"Zoom out");
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)view,L"&View");
    win=CreateWindowW(wc.lpszClassName,L"DyslexiReader",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,1100,850,nullptr,menu,instance,nullptr);
    if(voice) {
        voice->SetNotifyWindowMessage(win,WM_APP+1,0,0);
        ULONGLONG events=SPFEI(SPEI_WORD_BOUNDARY)|SPFEI(SPEI_END_INPUT_STREAM);
        voice->SetInterest(events,events);
    }
    ShowWindow(win,show);
    int argc;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argc>1) Load(argv[1]);LocalFree(argv);
    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)>0) {
        if(msg.message==WM_KEYDOWN && IsChild(win,msg.hwnd) &&
            (msg.wParam==VK_SPACE || msg.wParam==VK_ESCAPE || msg.wParam==VK_NEXT ||
             msg.wParam==VK_PRIOR || msg.wParam==VK_HOME || msg.wParam==VK_END ||
             msg.wParam==VK_UP || msg.wParam==VK_DOWN || (msg.wParam=='O' && GetKeyState(VK_CONTROL)<0)) &&
             IsWindowEnabled(win)) {
            SendMessageW(win,WM_KEYDOWN,msg.wParam,msg.lParam); continue;
        }
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    if(voice) voice->Release();FPDF_DestroyLibrary();CoUninitialize();return 0;
}


