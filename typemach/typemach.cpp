#include <iostream>
#include <fstream>
#include <Windows.h>
#include <ShellScalingApi.h>
#include <gdiplus.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

using namespace tesseract;
using namespace Gdiplus;
std::ifstream in("settings.ini");

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

void Screenshot(HDC screenDC, HDC memDC, HBITMAP hbmp, int ssX, int ssY, int ssSX, int ssSY)
{
    hbmp = CreateCompatibleBitmap(screenDC, ssSX, ssSY);
    SelectObject(memDC, hbmp);

    BitBlt(memDC, 0, 0, ssSX, ssSY, screenDC, ssX, ssY, SRCCOPY);

    CLSID encoderID;

    GetEncoderClsid(L"image/png", &encoderID);

    Bitmap* bmp = new Bitmap(hbmp, (HPALETTE)0);
    bmp->Save(L"text.png", &encoderID, NULL);
    delete bmp;
}

void ScaleCapturePoint(POINT& p)
{
    p.x *= (GetDpiForSystem() / 96);
    p.y *= (GetDpiForSystem() / 96);
}

void SetCapturePoint(POINT& p)
{
    bool init = false;
    while (!init)
    {
        if (GetAsyncKeyState(VK_F2))
        {
            GetCursorPos(&p);
            ScaleCapturePoint(p);
            init = true;
            Beep(1200, 200);
        }
    }
    std::cout << p.x << ", " << p.y << "\n\n";
}

char* DetectTextFromScreenshot(POINT& tl, POINT& br)
{
    char* outText;
    if (br.x <= tl.x || br.y <= tl.y)
    {
        std::cout << "Invalid screenshot coordinates!\n";
        return nullptr;
    }
    else
    {
        TessBaseAPI* api = new TessBaseAPI();
        char** configs = new char* [1];
        configs[0] = (char*)"char_config.txt";

        if (api->Init(NULL, "eng", OcrEngineMode::OEM_LSTM_ONLY, configs, 1, nullptr, nullptr, false))
        {
            std::cout << "Could not initialize tesseract.\n";
            return 0;
        }
        api->SetPageSegMode(PSM_AUTO);

        HDC hScreenDC = GetDC(NULL);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        HBITMAP hbmScreen = NULL;

        GdiplusStartupInput gdip;
        ULONG_PTR gdipToken;
        GdiplusStartup(&gdipToken, &gdip, NULL);

        Screenshot(hScreenDC, hMemoryDC, hbmScreen, tl.x, tl.y, br.x - tl.x, br.y - tl.y);

        Pix* image = pixRead("text.png");
        api->SetImage(image);
  
        outText = api->GetUTF8Text();

        api->End();
        api = nullptr;

        GdiplusShutdown(gdipToken);

        delete hbmScreen;
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        pixDestroy(&image);
        remove("text.png");
        return outText;
    }
}

int main()
{
    bool randomizedMs = false;
    int msPerKey = 16;

    //load settings
    char setting[16];
    while (in)
    {
        in >> setting;
        if (strstr(setting, "msPerKey="))
        {
            sscanf_s(setting + 9, "%d", &msPerKey);
            continue;
        }
        if (strstr(setting, "msRange="))
        {
            int a;
            sscanf_s(setting + 8, "%d", &a);
            randomizedMs = (bool)a;
            continue;
        }
    }
   
    POINT capture1 = { 0, 0 }, capture2 = { 0, 0 };

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    std::cout << "Set the capture points by moving your mouse to the top-left and bottom-right corners of the area that you wish to screenshot and pressing F2 for each corner. The bot will then detect the text that needs to be typed out.\n\nCapture point 1 (Top-left corner of screenshot): ";
    SetCapturePoint(capture1);

    std::cout << "Capture point 2 (Bottom-right corner of screenshot): ";

    //prevent key from being held after every SetCapturePoint
    while (GetAsyncKeyState(VK_F2))
        if (!GetAsyncKeyState(VK_F2))
            break;

    SetCapturePoint(capture2);
    
    char* detectedText = DetectTextFromScreenshot(capture1, capture2);
    
    if (detectedText == nullptr)
    {
        std::cout << "Invalid string!\n";
        return -1;
    }

    if (detectedText[0] == '1') //bandaid solution for underlined 'I' being detected as the character '1' idk lol
        detectedText[0] = 'I';

    std::cout << "Detected text: " << detectedText << "\n";

    Beep(1000, 800); //let the user know the text has been processed
    std::cout << "Press F2 to begin typing. You can press F4 while the bot is working to stop typing.\n";

    while (GetAsyncKeyState(VK_F2))
        if (!GetAsyncKeyState(VK_F2))
            break;

    //breaking out of this loop begins the typing
    while (!GetAsyncKeyState(VK_F2))
        if (GetAsyncKeyState(VK_F2))
            break;

    size_t textLen = strlen(detectedText);
    INPUT keyToPress, shiftKey;
    for(size_t i = 0; i < textLen; ++i)
    { 
        ZeroMemory(&keyToPress, sizeof(INPUT));
        ZeroMemory(&shiftKey, sizeof(INPUT));

        if (GetAsyncKeyState(VK_F4))
        {
            std::cout << "Forcefully stopped the bot.\n\n";
            break;
        }

        if (detectedText[i] == 'l' && i > 0 && i < textLen - 1 && detectedText[i + 1] == ' ' && detectedText[i - 1] == ' ') //another bandaid solution for when the pronoun 'I' is detected as the character 'l'
            detectedText[i] = 'I';

        keyToPress.type = INPUT_KEYBOARD;

        bool isUpper = detectedText[i] >= 'A' && detectedText[i] <= 'Z',
            isLower = detectedText[i] >= 'a' && detectedText[i] <= 'z',
            isNumber = detectedText[i] >= '0' && detectedText[i] <= '9';

        if (isUpper || isLower || isNumber) //convenient virtual-key codes for ascii letters and numbers (equal to ascii codes)
        {
            keyToPress.ki.wVk = detectedText[i] - 32 * isLower;
        }
        else
            switch (detectedText[i]) //switch case for every other character whose vk code isn't easy to get to in other ways
            {
                case ' ':
                    keyToPress.ki.wVk = VK_SPACE;
                    break;
                case '\n':
                    keyToPress.ki.wVk = VK_SPACE;
                    break;
                case ',':
                    keyToPress.ki.wVk = VK_OEM_COMMA;
                    break;
                case '.':
                    keyToPress.ki.wVk = VK_OEM_PERIOD;
                    break;
                case '!':
                    keyToPress.ki.wVk = 0x31; //'1' key
                    isUpper = true;
                    break;
                case '?':
                    keyToPress.ki.wVk = VK_OEM_2;
                    isUpper = true;
                    break;
                case ';':
                    keyToPress.ki.wVk = VK_OEM_1;
                    break;
                case ':':
                    keyToPress.ki.wVk = VK_OEM_1;
                    isUpper = true;
                    break;
                case '\'':
                    keyToPress.ki.wVk = VK_OEM_7;
                    break;
                case '"':
                    keyToPress.ki.wVk = VK_OEM_7;
                    isUpper = true;
                    break;
                case '-':
                    keyToPress.ki.wVk = VK_OEM_MINUS;
                    break;
                case '(':
                    keyToPress.ki.wVk = 0x39; //'9' key
                    isUpper = true;
                    break;
                case ')':
                    keyToPress.ki.wVk = 0x30; //'0' key
                    isUpper = true;
                default:
                    std::cout << "Invalid character '" << detectedText[i] << "'. Please restart the bot and try again.\n";
                    return -1;
            }

        if (isUpper)
        {
            shiftKey.type = INPUT_KEYBOARD;
            shiftKey.ki.wVk = VK_SHIFT;
            SendInput(1, &shiftKey, sizeof(INPUT));
            shiftKey.ki.dwFlags = KEYEVENTF_KEYUP;
        }

        SendInput(1, &keyToPress, sizeof(INPUT));

        keyToPress.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &keyToPress, sizeof(INPUT));
        if (isUpper) SendInput(1, &shiftKey, sizeof(INPUT));

        if (randomizedMs)
            Sleep(rand() % (2 * msPerKey + 1));
        else
            Sleep(msPerKey);
    }
   
    return 0;
}
