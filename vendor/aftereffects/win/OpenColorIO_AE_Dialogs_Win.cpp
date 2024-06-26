// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.


#include "OpenColorIO_AE_Dialogs.h"

#include <list>

#include <Windows.h>
#include <Shlobj.h>
#include <Icm.h>
#include <assert.h>

#include "lcms2.h"


static HINSTANCE hDllInstance = NULL;

static void AppendString(char *text, int &length, const char *str, int len = -1)
{
    if(len < 0)
        len = strlen(str);

    const char *in = str;
    char *out = &text[length];

    for(int i=0; i < len; i++)
    {
        *out++ = *in++;

        length++;
    }
}

static void AppendNull(char *text, int &length)
{
    AppendString(text, length, "\0\0", 1);
}

static void MakeFilterText(char *filter_text,
                            const ExtensionMap &extensions,
                            bool do_combined)
{
    // Construct the Windows file dialog filter string, which looks like this:
    //
    //  "All OCIO files\0"
    //      "*.ocio;*.cube;*.vf;*.mga\0"
    //  "OpenColorIO (*.ocio)\0"
    //      "*.ocio\0"
    //  "Iridas (*.cube)\0"
    //      "*.cube\0"
    //  "Nuke Vectorfield (*.vf)\0"
    //      "*.vf\0"
    //  "Apple Color (*.mga)\0"
    //      "*.mga\0"
    //  "\0";
    //
    // Note the inline nulls and final double-null, which foil regular string functions.

    char combined_entry[512];
    int combined_length = 0;

    char seperate_entries[1024];
    int seperate_length = 0;

    AppendString(combined_entry, combined_length, "All OCIO files");
    AppendNull(combined_entry, combined_length);

    for(ExtensionMap::const_iterator i = extensions.begin(); i != extensions.end(); i++)
    {
        std::string extension = i->first;
        std::string format = i->second;

        std::string format_part = format + " (*." + extension + ")";
        std::string extension_part = "*." + extension;
        std::string combined_part = extension_part + ";";

        AppendString(seperate_entries, seperate_length, format_part.c_str(), format_part.size());
        AppendNull(seperate_entries, seperate_length);
        AppendString(seperate_entries, seperate_length, extension_part.c_str(), extension_part.size());
        AppendNull(seperate_entries, seperate_length);

        AppendString(combined_entry, combined_length, combined_part.c_str(), combined_part.size());
    }

    AppendNull(seperate_entries, seperate_length);
    AppendNull(combined_entry, combined_length);


    assert(((do_combined ? combined_length : 0) + seperate_length) < 1024);

    char *in = combined_entry;
    char *out = filter_text;

    if(do_combined)
    {
        assert(combined_length < 512);

        for(int i=0; i < combined_length; i++)
            *out++ = *in++;
    }

    in = seperate_entries;

    assert(seperate_length < 1024);

    for(int i=0; i < seperate_length; i++)
        *out++ = *in++;
}


// Oy, what a pain in the neck.  AE uses a 4 byte struct alignment, which messes with
// GetSaveFileName because it's struct sensitive (especially the part about the struct size).
// This pragma temporarily sets us back to 8 bytes, but since OPENFILENAME has already been
// victimized, we copy it here and replace it with our own myOPENFILENAME.
#pragma pack(push, 8)
typedef struct mytagOFNA {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HINSTANCE    hInstance;
    LPCSTR       lpstrFilter;
    LPSTR        lpstrCustomFilter;
    DWORD        nMaxCustFilter;
    DWORD        nFilterIndex;
    LPSTR        lpstrFile;
    DWORD        nMaxFile;
    LPSTR        lpstrFileTitle;
    DWORD        nMaxFileTitle;
    LPCSTR       lpstrInitialDir;
    LPCSTR       lpstrTitle;
    DWORD        Flags;
    WORD         nFileOffset;
    WORD         nFileExtension;
    LPCSTR       lpstrDefExt;
    LPARAM       lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCSTR       lpTemplateName;
#ifdef _MAC
    LPEDITMENU   lpEditInfo;
    LPCSTR       lpstrPrompt;
#endif
#if (_WIN32_WINNT >= 0x0500)
    void *        pvReserved;
    DWORD        dwReserved;
    DWORD        FlagsEx;
#endif // (_WIN32_WINNT >= 0x0500)
} myOPENFILENAMEA, *myLPOPENFILENAMEA;
typedef struct mytagOFNW {
    DWORD        lStructSize;
    HWND         hwndOwner;
    HINSTANCE    hInstance;
    LPCWSTR      lpstrFilter;
    LPWSTR       lpstrCustomFilter;
    DWORD        nMaxCustFilter;
    DWORD        nFilterIndex;
    LPWSTR       lpstrFile;
    DWORD        nMaxFile;
    LPWSTR       lpstrFileTitle;
    DWORD        nMaxFileTitle;
    LPCWSTR      lpstrInitialDir;
    LPCWSTR      lpstrTitle;
    DWORD        Flags;
    WORD         nFileOffset;
    WORD         nFileExtension;
    LPCWSTR      lpstrDefExt;
    LPARAM       lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCWSTR      lpTemplateName;
#ifdef _MAC
    LPEDITMENU   lpEditInfo;
    LPCSTR       lpstrPrompt;
#endif
#if (_WIN32_WINNT >= 0x0500)
    void *        pvReserved;
    DWORD        dwReserved;
    DWORD        FlagsEx;
#endif // (_WIN32_WINNT >= 0x0500)
} myOPENFILENAMEW, *myLPOPENFILENAMEW;
#ifdef UNICODE
typedef myOPENFILENAMEW myOPENFILENAME;
typedef myLPOPENFILENAMEW myLPOPENFILENAME;
#else
typedef myOPENFILENAMEA myOPENFILENAME;
typedef myLPOPENFILENAMEA myLPOPENFILENAME;
#endif // UNICODE

bool OpenFile(char *path, int buf_len, const ExtensionMap &extensions, const void *hwnd)
{
    const char *my_lpstrTitle = "Import OCIO";
    const char *my_lpstrDefExt = "ocio";

    char my_lpstrFilter[1024];
    MakeFilterText(my_lpstrFilter, extensions, true);


    myOPENFILENAME lpofn;

    lpofn.lStructSize = sizeof(lpofn);
    lpofn.hwndOwner = (HWND)hwnd;
    lpofn.hInstance = hDllInstance;
    lpofn.lpstrFilter = my_lpstrFilter;
    lpofn.lpstrCustomFilter = NULL;
    lpofn.nMaxCustFilter = 0;
    lpofn.nFilterIndex = 0;
    lpofn.lpstrFile = path;
    lpofn.nMaxFile = buf_len;
    lpofn.lpstrFileTitle = path;
    lpofn.nMaxFileTitle = buf_len;
    lpofn.lpstrInitialDir = NULL;
    lpofn.lpstrTitle = my_lpstrTitle;
    lpofn.Flags = OFN_LONGNAMES |
                    OFN_HIDEREADONLY | 
                    OFN_PATHMUSTEXIST |
                    OFN_OVERWRITEPROMPT;
    lpofn.nFileOffset = 0;
    lpofn.nFileExtension = 0;
    lpofn.lpstrDefExt = my_lpstrDefExt;
    lpofn.lCustData = 0;
    lpofn.lpfnHook = NULL;
    lpofn.lpTemplateName = NULL;
    lpofn.lStructSize = sizeof(lpofn);

    return GetOpenFileName((LPOPENFILENAME)&lpofn);
}


bool SaveFile(char *path, int buf_len, const ExtensionMap &extensions, const void *hwnd)
{
    const char *my_lpstrTitle = "Export OCIO";
    const char *my_lpstrDefExt = "icc";

    char my_lpstrFilter[512];
    MakeFilterText(my_lpstrFilter, extensions, false);


    myOPENFILENAME lpofn;

    lpofn.lStructSize = sizeof(lpofn);
    lpofn.hwndOwner = (HWND)hwnd;
    lpofn.hInstance = hDllInstance;
    lpofn.lpstrFilter = my_lpstrFilter;
    lpofn.lpstrCustomFilter = NULL;
    lpofn.nMaxCustFilter = 0;
    lpofn.nFilterIndex = 0;
    lpofn.lpstrFile = path;
    lpofn.nMaxFile = buf_len;
    lpofn.lpstrFileTitle = path;
    lpofn.nMaxFileTitle = buf_len;
    lpofn.lpstrInitialDir = NULL;
    lpofn.lpstrTitle = my_lpstrTitle;
    lpofn.Flags = OFN_LONGNAMES |
                    OFN_HIDEREADONLY | 
                    OFN_PATHMUSTEXIST |
                    OFN_OVERWRITEPROMPT;
    lpofn.nFileOffset = 0;
    lpofn.nFileExtension = 0;
    lpofn.lpstrDefExt = my_lpstrDefExt;
    lpofn.lCustData = 0;
    lpofn.lpfnHook = NULL;
    lpofn.lpTemplateName = NULL;
    lpofn.lStructSize = sizeof(lpofn);

    return GetSaveFileName((LPOPENFILENAME)&lpofn);
}
#pragma pack(pop)

// dialog item IDs
enum {
    DLOG_noUI = -1,
    DLOG_OK = IDOK, // was 1
    DLOG_Cancel = IDCANCEL, // was 2
    DLOG_Profile_Menu = 3
};


static std::vector<std::string> *g_profile_vec = NULL;
static int g_selected_item = DLOG_noUI;

static WORD g_item_clicked = 0;

static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch (message) 
    { 
      case WM_INITDIALOG:
        do{
            // add profile list to combo boxe
            HWND menu = GetDlgItem(hwndDlg, DLOG_Profile_Menu);

            for(int i=0; i < g_profile_vec->size(); i++)
            {
                SendMessage(menu, (UINT)CB_ADDSTRING,
                            (WPARAM)wParam, (LPARAM)(LPCTSTR)g_profile_vec->at(i).c_str() );
                            
                SendMessage(menu,(UINT)CB_SETITEMDATA,
                            (WPARAM)i, (LPARAM)(DWORD)i); // channel index number

                if( g_selected_item == i )
                    SendMessage(menu, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
            }
        }while(0);
        return FALSE;
 
        case WM_COMMAND: 
            g_item_clicked = LOWORD(wParam);

            switch(LOWORD(wParam)) 
            { 
                case DLOG_OK: 
                case DLOG_Cancel:  // do the same thing, but g_item_clicked differ
                    do{
                        HWND menu = GetDlgItem(hwndDlg, DLOG_Profile_Menu);

                        LRESULT cur_sel = SendMessage(menu, (UINT)CB_GETCURSEL,
                                                        (WPARAM)0, (LPARAM)0);

                        g_selected_item = SendMessage(menu, (UINT)CB_GETITEMDATA,
                                                        (WPARAM)cur_sel, (LPARAM)0);

                    }while(0);

                    EndDialog(hwndDlg, 0);
                    return TRUE; 
            } 
    }

    return FALSE; 
} 

bool GetMonitorProfile(char *path, int buf_len, const void *hwnd)
{
    std::list<std::string> profile_descriptions;
    std::map<std::string, std::string> profile_paths;

    // path to the monitor's profile
    char monitor_profile_path[256] = { '\0' };
    DWORD path_size = 256;
    BOOL get_icm_result = GetICMProfile(GetDC((HWND)hwnd), &path_size, monitor_profile_path);

    // directory where Windows stores its profiles
    char profile_directory[256] = { '\0' };
    DWORD dir_name_size = 256;
    BOOL get_color_dir_result = GetColorDirectory(NULL, profile_directory, &dir_name_size);

    // Get the profile file names from Windows
    ENUMTYPE enum_type;
    enum_type.dwSize = sizeof(ENUMTYPE);
    enum_type.dwVersion = ENUM_TYPE_VERSION;
    enum_type.dwFields = ET_DEVICECLASS;  // alternately could use ET_CLASS
    enum_type.dwDeviceClass = CLASS_MONITOR;

    BYTE *buf = NULL;
    DWORD buf_size = 0;
    DWORD num_profiles = 0;

    BOOL other_enum_result = EnumColorProfiles(NULL, &enum_type,
                                                buf, &buf_size, &num_profiles);

    if(buf_size > 0 && num_profiles > 0)
    {
        buf = (BYTE *)malloc(buf_size);

        other_enum_result = EnumColorProfiles(NULL, &enum_type,
                                                buf, &buf_size, &num_profiles);

        if(other_enum_result)
        {
            // build a list of the profile descriptions
            // and a map to return the paths
            char *prof_name = (char *)buf;

            for(int i=0; i < num_profiles; i++)
            {
                std::string prof = prof_name;
                std::string prof_path = std::string(profile_directory) + "\\" + prof_name;

                cmsHPROFILE hProfile = cmsOpenProfileFromFile(prof_path.c_str(), "r");

                // Note: Windows will give us profiles that aren't ICC (.cdmp for example).
                // Don't worry, LittleCMS will just return NULL for those.
                if(hProfile)
                {
                    char profile_description[256];

                    cmsUInt32Number got_desc = cmsGetProfileInfoASCII(hProfile,
                                                                    cmsInfoDescription,
                                                                    "en", "US",
                                                                    profile_description,
                                                                    256);

                    if(got_desc)
                    {
                        profile_descriptions.push_back(profile_description);

                        profile_paths[ profile_description ] = prof_path;
                    }

                    cmsCloseProfile(hProfile);
                }

                prof_name += strlen(prof_name) + 1;
            }
        }

        free(buf);
    }


    if(profile_descriptions.size() > 0)
    {
        // set a vector and selected index for building the profile menu
        profile_descriptions.sort();
        profile_descriptions.unique();

        std::vector<std::string> profile_vec;
        int selected = 0;

        for(std::list<std::string>::const_iterator i = profile_descriptions.begin(); i != profile_descriptions.end(); i++)
        {
            profile_vec.push_back( *i );

            if( profile_paths[ *i ] == monitor_profile_path)
            {
                selected = profile_vec.size() - 1;
            }
        }

        // run the dialog
        g_profile_vec = &profile_vec;
        g_selected_item = selected;

        int status = DialogBox(hDllInstance, (LPSTR)"PROFILEDIALOG",
                                (HWND)hwnd, (DLGPROC)DialogProc);


        if(status == -1)
        {
            // dialog didn't open, my bad
            return true;
        }
        else if(g_item_clicked == DLOG_Cancel)
        {
            return false;
        }
        else
        {
            strncpy(path, profile_paths[ profile_vec[ g_selected_item ] ].c_str(), buf_len);

            return true;
        }
    }
    else
        return true;
}


int PopUpMenu(const MenuVec &menu_items, int selected_index, const void *hwnd)
{
    HMENU menu = CreatePopupMenu();

    if(menu)
    {
        for(int i=0; i < menu_items.size(); i++)
        {
            std::string label = menu_items[i];

            UINT flags = (i == selected_index ? (MF_STRING | MF_CHECKED) : MF_STRING);

            if(label == "(-")
            {
                flags |= MF_SEPARATOR;
            }
            else if(label == "$OCIO")
            {
                char *file = std::getenv("OCIO"); // not using OpenColorIO_AE_Context::getenvOCIO() here so Photoshop can use this file too 

                if(file == NULL)
                    flags |= MF_GRAYED;
            }
            else if(label == "(nada)")
            {
                flags |= MF_GRAYED;

                char appdata_path[MAX_PATH];
                HRESULT result = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
                                                    SHGFP_TYPE_CURRENT, appdata_path);

                if(result == S_OK)
                {
                    label = "No configs in " + std::string(appdata_path) + "\\OpenColorIO\\";
                }
            }

            AppendMenu(menu, flags, i + 1, label.c_str());
        }

        POINT pos;
        GetCursorPos(&pos);

        int result = TrackPopupMenuEx(menu,
                            (TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD),
                            pos.x, pos.y, (HWND)hwnd, NULL);

        DestroyMenu(menu);

        if(result == 0)
        {
            // means the user clicked off the menu
            return selected_index;
        }
        else
            return result - 1;
    }
    else
        return selected_index;
}


static void
tokenize(std::vector<std::string> &tokens, 
         const std::string& str, 
         std::string delimiters)
{
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (pos != std::string::npos || lastPos != std::string::npos)
    {
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = str.find_first_not_of(delimiters, pos);
        pos = str.find_first_of(delimiters, lastPos);
    }
}


static bool compare_nocase(const std::string &first, const std::string &second)
{
  unsigned int i=0;

  while( (i < first.length()) && (i < second.length()) )
  {
    if(tolower(first[i]) < tolower(second[i]))
        return true;
    else if(tolower(first[i]) > tolower(second[i]))
        return false;

    ++i;
  }

  return ( first.length() < second.length() );
}

bool ColorSpacePopUpMenu(OCIO::ConstConfigRcPtr config, std::string &colorSpace, bool selectRoles, const void *hwnd)
{
    HMENU menu = CreatePopupMenu();

    typedef std::list<std::string> CatList;
    typedef std::map<std::string, CatList> CatMap;

    CatMap categoriesMap;
    CatMap encodingsMap;

    for(int i=0; i < config->getNumColorSpaces(); ++i)
    {
        const char *colorSpaceName = config->getColorSpaceNameByIndex(i);
        
        OCIO::ConstColorSpaceRcPtr colorSpacePtr = config->getColorSpace(colorSpaceName);
        
        const char *family = colorSpacePtr->getFamily();
        
        
        std::vector<std::string> pathComponents;
        
        if(family != NULL && family != std::string(""))
        {
            tokenize(pathComponents, family, std::string(1, config->getFamilySeparator()));
        }
        
        pathComponents.push_back(colorSpaceName);
        
        
        HMENU currentMenu = menu;
        
        for(int j=0; j < pathComponents.size(); j++)
        {
            const std::string &componentName = pathComponents[j];
            
            if(j == (pathComponents.size() - 1))
            {
                UINT flags = MF_STRING;

                if(componentName == colorSpace)
                    flags |= MF_CHECKED;

                const BOOL inserted = AppendMenu(currentMenu, flags, i + 1, componentName.c_str());

                assert(inserted);
            }
            else
            {
                int componentMenuPos = -1;

                for(int k=0; k < GetMenuItemCount(currentMenu) && componentMenuPos < 0; k++)
                {
                    CHAR buf[256];

                    const int strLen = GetMenuString(currentMenu, k, buf, 255, MF_BYPOSITION);

                    assert(strLen > 0);

                    if(componentName == buf)
                        componentMenuPos = k;
                }

                if(componentMenuPos < 0)
                {
                    HMENU subMenu = CreateMenu();
                    
                    const BOOL inserted = AppendMenu(currentMenu, MF_STRING | MF_POPUP, (UINT_PTR)subMenu, componentName.c_str());

                    assert(inserted);

                    componentMenuPos = (GetMenuItemCount(currentMenu) - 1);
                }
                
                currentMenu = GetSubMenu(currentMenu, componentMenuPos);
            }
        }

        for(int j=0; j < colorSpacePtr->getNumCategories(); j++)
        {
            const char *categoryName = colorSpacePtr->getCategory(j);

            categoriesMap[categoryName].push_back(colorSpaceName);
        }

        const char *encoding = colorSpacePtr->getEncoding();

        if(encoding != NULL && encoding != std::string(""))
        {
            encodingsMap[encoding].push_back(colorSpaceName);
        }
    }
    
    if(encodingsMap.size() > 0 || categoriesMap.size() > 0 || config->getNumRoles() > 0)
    {
        const BOOL dividerInserted = InsertMenu(menu, 0, MF_STRING | MF_BYPOSITION | MF_SEPARATOR, 0, "Sep");

        assert(dividerInserted);
    }

    if(encodingsMap.size() > 0)
    {
        HMENU encodingsMenu = CreatePopupMenu();

        const BOOL encodingsInserted = InsertMenu(menu, 0, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT_PTR)encodingsMenu, "Encodings");

        assert(encodingsInserted);

        CatList encodingsList;

        for(CatMap::const_iterator i = encodingsMap.begin(); i != encodingsMap.end(); ++i)
        {
            encodingsList.push_back(i->first);
        }

        encodingsList.sort(compare_nocase);

        for(CatList::const_iterator i = encodingsList.begin(); i != encodingsList.end(); ++i)
        {
            const std::string &encoding = *i;

            CatList &spacesList = encodingsMap[encoding];

            spacesList.sort(compare_nocase);

            HMENU encodingSubmenu = CreatePopupMenu();

            const BOOL encodingInserted = AppendMenu(encodingsMenu, MF_STRING | MF_POPUP, (UINT_PTR)encodingSubmenu, encoding.c_str());

            assert(encodingInserted);

            for(CatList::const_iterator j = spacesList.begin(); j != spacesList.end(); ++j)
            {
                const std::string &colorSpaceName = *j;

                int colorSpaceIndex = -1;

                for(int k=0; k < config->getNumColorSpaces() && colorSpaceIndex < 0; k++)
                {
                    const std::string colorSpaceName2 = config->getColorSpaceNameByIndex(k);

                    if(colorSpaceName2 == colorSpaceName)
                        colorSpaceIndex = k;
                }

                UINT colorSpaceFlags = MF_STRING;

                if(colorSpaceName == colorSpace)
                    colorSpaceFlags |= MF_CHECKED;

                const BOOL colorSpaceInsterted = AppendMenu(encodingSubmenu, colorSpaceFlags, colorSpaceIndex + 1, colorSpaceName.c_str());

                assert(colorSpaceInsterted);
            }
        }
    }

    if(categoriesMap.size() > 0)
    {
        HMENU categoriesMenu = CreatePopupMenu();

        const BOOL categoriesInserted = InsertMenu(menu, 0, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT_PTR)categoriesMenu, "Categories");

        assert(categoriesInserted);

        CatList categoriesList;

        for(CatMap::const_iterator i = categoriesMap.begin(); i != categoriesMap.end(); ++i)
        {
            categoriesList.push_back(i->first);
        }

        categoriesList.sort(compare_nocase);

        for(CatList::const_iterator i = categoriesList.begin(); i != categoriesList.end(); ++i)
        {
            const std::string &category = *i;

            CatList &spacesList = categoriesMap[category];

            spacesList.sort(compare_nocase);

            HMENU categorySubmenu = CreatePopupMenu();

            const BOOL categoryInserted = AppendMenu(categoriesMenu, MF_STRING | MF_POPUP, (UINT_PTR)categorySubmenu, category.c_str());

            assert(categoryInserted);

            for(CatList::const_iterator j = spacesList.begin(); j != spacesList.end(); ++j)
            {
                const std::string &colorSpaceName = *j;

                int colorSpaceIndex = -1;

                for(int k=0; k < config->getNumColorSpaces() && colorSpaceIndex < 0; k++)
                {
                    const std::string colorSpaceName2 = config->getColorSpaceNameByIndex(k);

                    if(colorSpaceName2 == colorSpaceName)
                        colorSpaceIndex = k;
                }

                UINT colorSpaceFlags = MF_STRING;

                if(colorSpaceName == colorSpace)
                    colorSpaceFlags |= MF_CHECKED;

                const BOOL colorSpaceInsterted = AppendMenu(categorySubmenu, colorSpaceFlags, colorSpaceIndex + 1, colorSpaceName.c_str());

                assert(colorSpaceInsterted);
            }
        }
    }

    if(config->getNumRoles() > 0)
    {
        HMENU rolesMenu = CreatePopupMenu();

        const BOOL rolesInserted = InsertMenu(menu, 0, MF_STRING | MF_BYPOSITION | MF_POPUP, (UINT_PTR)rolesMenu, "Roles");

        assert(rolesInserted);
        
        for(int i=0; i < config->getNumRoles(); i++)
        {
            const std::string roleName = config->getRoleName(i);
            
            OCIO::ConstColorSpaceRcPtr colorSpacePtr = config->getColorSpace(roleName.c_str());
            
            const std::string colorSpaceName = colorSpacePtr->getName();
            
            int colorSpaceIndex = -1;

            for(int k=0; k < config->getNumColorSpaces() && colorSpaceIndex < 0; k++)
            {
                const std::string colorSpaceName2 = config->getColorSpaceNameByIndex(k);

                if(colorSpaceName2 == colorSpaceName)
                    colorSpaceIndex = k;
            }

            HMENU roleSubmenu = CreatePopupMenu();
            
            UINT roleFlags = MF_STRING | MF_POPUP;

            if(selectRoles && roleName == colorSpace)
                roleFlags |= MF_CHECKED;

            const BOOL roleInserted = AppendMenu(rolesMenu, roleFlags, (UINT_PTR)roleSubmenu, roleName.c_str());

            assert(roleInserted);
            
            UINT colorSpaceFlags = MF_STRING;

            if(colorSpaceName == colorSpace)
                colorSpaceFlags |= MF_CHECKED;

            const BOOL colorSpaceInsterted = AppendMenu(roleSubmenu, colorSpaceFlags, colorSpaceIndex + 1, colorSpaceName.c_str());

            assert(colorSpaceInsterted);
        }
    }

    POINT pos;
    GetCursorPos(&pos);

    int result = TrackPopupMenuEx(menu,
                        (TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD),
                        pos.x, pos.y, (HWND)hwnd, NULL);

    DestroyMenu(menu);

    if(result > 0)
    {
        colorSpace = config->getColorSpaceNameByIndex(result - 1);

        return true;
    }
    else
        return false;
}


void GetStdConfigs(ConfigVec &configs)
{
    char appdata_path[MAX_PATH];

    HRESULT result = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
                                        SHGFP_TYPE_CURRENT, appdata_path);

    if(result == S_OK)
    {
        std::string dir_path = std::string(appdata_path) + "\\OpenColorIO\\";

        std::string search_path = dir_path + "*";

        WIN32_FIND_DATA find_data;

        HANDLE searchH = FindFirstFile(search_path.c_str(), &find_data);

        if(searchH != INVALID_HANDLE_VALUE)
        {
            if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                std::string config_path = dir_path + find_data.cFileName + "\\config.ocio";

                WIN32_FIND_DATA find_data_temp;

                HANDLE fileH = FindFirstFile(config_path.c_str(), &find_data_temp);

                if(fileH != INVALID_HANDLE_VALUE)
                {
                    configs.push_back(find_data.cFileName);

                    FindClose(fileH);
                }
            }

            while( FindNextFile(searchH, &find_data) )
            {
                if(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    std::string config_path = dir_path + find_data.cFileName + "\\config.ocio";

                    WIN32_FIND_DATA find_data_temp;

                    HANDLE fileH = FindFirstFile(config_path.c_str(), &find_data_temp);

                    if(fileH != INVALID_HANDLE_VALUE)
                    {
                        configs.push_back(find_data.cFileName);

                        FindClose(fileH);
                    }
                }
            }

            FindClose(searchH);
        }
    }
}


std::string GetStdConfigPath(const std::string &name)
{
    char appdata_path[MAX_PATH];

    HRESULT result = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL,
                                        SHGFP_TYPE_CURRENT, appdata_path);

    if(result == S_OK)
    {
        std::string config_path = std::string(appdata_path) + "\\OpenColorIO\\" +
                                name + "\\config.ocio";

        WIN32_FIND_DATA find_data;

        HANDLE searchH = FindFirstFile(config_path.c_str(), &find_data);

        if(searchH != INVALID_HANDLE_VALUE)
        {
            FindClose(searchH);

            return config_path;
        }
    }

    return "";
}


void ErrorMessage(const char *message , const void *hwnd)
{
    MessageBox((HWND)hwnd, message, "OpenColorIO", MB_OK);
}

#ifdef SUPPLY_HINSTANCE
void SetHInstance(void *hInstance)
{
    hDllInstance = (HINSTANCE)hInstance;
}
#else
BOOL WINAPI DllMain(HANDLE hInstance, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        hDllInstance = (HINSTANCE)hInstance;

    return TRUE;
}
#endif