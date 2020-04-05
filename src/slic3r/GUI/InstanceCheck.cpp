#include "GUI_App.hpp"
#include "InstanceCheck.hpp"

#include "boost/nowide/convert.hpp"
#include <boost/log/trivial.hpp>
#include <iostream>

#include <fcntl.h>
#include <errno.h>

#if __linux__
#include <dbus/dbus.h> /* Pull in all of D-Bus headers. */
#endif //__linux__

#if _WIN32 
//win32 callbacks
//catching message from another instance
static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TCHAR lpClassName[1000];
	GetClassName(hWnd, lpClassName, 100);
	switch (message) {
	case WM_COPYDATA:
	{
		COPYDATASTRUCT* copy_data_structure = { 0 };
		copy_data_structure = (COPYDATASTRUCT*)lParam;
		if (copy_data_structure->dwData == 1) {
			LPCWSTR arguments = (LPCWSTR)copy_data_structure->lpData;
			//Slic3r::InstanceCheck::instance_check().handle_message(boost::nowide::narrow(arguments));
			Slic3r::GUI::wxGetApp().other_instance_message_handler()->handle_message(boost::nowide::narrow(arguments));
		}

	}
	break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

static HWND PrusaSlicerHWND;
static BOOL CALLBACK EnumWindowsProc(_In_ HWND   hwnd, _In_ LPARAM lParam)
{
	//checks for other instances of prusaslicer, if found brings it to front and return false to stop enumeration and quit this instance
	//search is done by classname(wxWindowNR is wxwidgets thing, so probably not unique) and name in window upper panel
	//other option would be do a mutex and check for its existence
	TCHAR wndText[1000];
	TCHAR className[1000];
	GetClassName(hwnd, className, 1000);
	GetWindowText(hwnd, wndText, 1000);
	std::wstring classNameString(className);
	std::wstring wndTextString(wndText);
	if (wndTextString.find(L"PrusaSlicer") != std::wstring::npos && classNameString == L"wxWindowNR") {
		//std::wcout << L"found " << wndTextString << std::endl;
		PrusaSlicerHWND = hwnd;
		ShowWindow(hwnd, SW_SHOWMAXIMIZED);
		SetForegroundWindow(hwnd);
		return false;
	}
	return true;
}

static void create_listener_window()
{
	WNDCLASSEX wndClass = { 0 };
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.hInstance = reinterpret_cast<HINSTANCE>(GetModuleHandle(0));
	wndClass.lpfnWndProc = reinterpret_cast<WNDPROC>(WndProc);//this is callback
	wndClass.lpszClassName = L"PrusaSlicer_single_instance_listener_class";
	if (!RegisterClassEx(&wndClass)) {
		DWORD err = GetLastError();
		return;
	}

	HWND hWnd = CreateWindowEx(
		0,//WS_EX_NOACTIVATE,
		L"PrusaSlicer_single_instance_listener_class",
		L"PrusaSlicer_listener_window",
		WS_OVERLAPPEDWINDOW,//WS_DISABLED, // style
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL, NULL,
		GetModuleHandle(NULL),
		NULL);
	if (hWnd == NULL) {
		DWORD err = GetLastError();
	}
	else {
		//ShowWindow(hWnd, SW_SHOWNORMAL);
		UpdateWindow(hWnd);
	}
}
#endif //_WIN32 

namespace Slic3r {
namespace instance_check_internal
{
	struct CommandLineAnalysis
	{
		bool           should_send;
		std::string    cl_string;
	};
	static CommandLineAnalysis process_command_line(int argc, char** argv)
	{
		CommandLineAnalysis ret{ false };
		if (argc == 0)
			return ret;
		ret.cl_string = argv[0];
		for (size_t i = 1; i < argc; i++) {
			//		if (argv[i][0] == '-')
			//		{
			//			if(argv[i][1] == 's')
			ret.should_send = true;
			//		} else {
			ret.cl_string += " ";
			ret.cl_string += argv[i];
			//		}
		}
		return ret;
	}
} //namespace instance_check_internal

#if _WIN32

namespace instance_check_internal
{
	static void send_message(const HWND hwnd)
	{
		LPWSTR command_line_args = GetCommandLine();
		//Create a COPYDATASTRUCT to send the information
		//cbData represents the size of the information we want to send.
		//lpData represents the information we want to send.
		//dwData is an ID defined by us(this is a type of ID different than WM_COPYDATA).
		COPYDATASTRUCT data_to_send = { 0 };
		data_to_send.dwData = 1;
		data_to_send.cbData = sizeof(TCHAR) * (wcslen(command_line_args) + 1);
		data_to_send.lpData = command_line_args;

		SendMessage(hwnd, WM_COPYDATA, 0, (LPARAM)&data_to_send);
	}
} //namespace instance_check_internal

bool instance_check(int argc, char** argv)
{
	instance_check_internal::CommandLineAnalysis cla = instance_check_internal::process_command_line(argc, argv);
	if (cla.should_send) {
		// Call EnumWidnows with own callback. cons: Based on text in the name of the window and class name which is generic.
		if (!EnumWindows(EnumWindowsProc, 0)) {
			instance_check_internal::send_message(PrusaSlicerHWND);
			//printf("Another instance of PrusaSlicer is already running.\n");
			HWND hwndListener;
			if ((hwndListener = FindWindow(NULL, L"PrusaSlicer_listener_window")) != NULL) {
				//instance_check_internal::send_message(hwndListener);
			}
			else {
				//printf("Listener window not found - teminating without sent info.\n");
			}
			return true;
		}
	}
	return false;
}

#elif defined(__APPLE__)

namespace instance_check_internal
{
	static int get_lock() 
	{
		struct flock fl;
		int fdlock;
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 1;

		if ((fdlock = open("/tmp/prusaslicer.lock", O_WRONLY | O_CREAT, 0666)) == -1)
			return 0;

		if (fcntl(fdlock, F_SETLK, &fl) == -1)
			return 0;

		return 1;
	}
} //namespace instance_check_internal

bool instance_check(int argc, char** argv)
{
	if (!instance_check_internal::get_lock()) {
		std::cout << "Process already running!" << std::endl;
		send_message_mac("message");
		return true;
	}
	return false;
}

#elif defined(__linux__)

namespace instance_check_internal
{
	static int get_lock() 
	{
		struct flock fl;
		int fdlock;
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 1;

		if ((fdlock = open("/tmp/prusaslicer.lock", O_WRONLY | O_CREAT, 0666)) == -1)
			return 0;

		if (fcntl(fdlock, F_SETLK, &fl) == -1)
			return 0;

		return 1;
	}
	static std::string get_pid_string_by_name(const std::string procName)
	{
		int pid = -1;
		std::string pid_string = "";
		// Open the /proc directory
		DIR* dp = opendir("/proc");
		if (dp != NULL)
		{
			// Enumerate all entries in directory until process found
			struct dirent* dirp;
			while (pid < 0 && (dirp = readdir(dp)))
			{
				// Skip non-numeric entries
				int id = atoi(dirp->d_name);
				if (id > 0) {
					// Read contents of virtual /proc/{pid}/cmdline file
					std::string cmdPath = std::string("/proc/") + dirp->d_name + "/cmdline";
					std::ifstream cmdFile(cmdPath.c_str());
					std::string cmdLine;
					getline(cmdFile, cmdLine);
					if (!cmdLine.empty()) {
						// Keep first cmdline item which contains the program path
						size_t pos = cmdLine.find('\0');
						if (pos != std::string::npos)
							cmdLine = cmdLine.substr(0, pos);
						// Keep program name only, removing the path
						pos = cmdLine.rfind('/');
						if (pos != std::string::npos)
							cmdLine = cmdLine.substr(pos + 1);
						// Compare against requested process name
						if (cmdLine.find(procName) != std::string::npos) {
							pid = id;
							pid_string = dirp->d_name;
						}
					}

				}
			}
		}

		closedir(dp);

		return pid_string;
	}
	static void send_message()
	{
	}
} //namespace instance_check_internal

bool instance_check(int argc, char** argv)
{
	if (!instance_check_internal::get_lock()) {
		std::cout << "Process already running!" << std::endl;
		std::string pid_string = instance_check_internal::get_pid_string_by_name("prusa-slicer");
		if (pid_string != "") {
			std::cout << "pid " << pid_string << std::endl;
			instance_check_internal::send_message();
		}
		return true;
	}
	return false;
}
#endif //_WIN32/__APPLE__/__linux__

//listen dbus
	/*
	DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    int ret;
    char* sigvalue;

    printf("Listening for signals\n");

    // initialise the errors
    dbus_error_init(&err);
    
    // connect to the bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err); 
    }
    if (NULL == conn) { 
        return;
    }
   
    // request our name on the bus and check for errors
    ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Listen - Name Error %s(%s)\n", err.name, err.message);
        dbus_error_free(&err); 
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
        return;
    }

    // add a rule for which messages we want to see
    dbus_bus_add_match(conn, "type='signal',interface='org.freedesktop.Notifications'", &err); // see signals from the given interface
    dbus_connection_flush(conn);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Match Error (%s)\n", err.message);
        return; 
    }
    printf("Match rule sent\n");

    // loop listening for signals being emmitted
    while (true) {

        // non blocking read of the next available message
        dbus_connection_read_write(conn, 0);
        msg = dbus_connection_pop_message(conn);

        // loop again if we haven't read a message
        if (NULL == msg) { 
            sleep(1);
            continue;
        }

        // check if the message is a signal from the correct interface and with the correct name
        if (dbus_message_is_signal(msg, "test.signal.Type", "Test")) {
         
            // read the parameters
            if (!dbus_message_iter_init(msg, &args))
                fprintf(stderr, "Message Has No Parameters\n");
            else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args)) 
                fprintf(stderr, "Argument is not string!\n"); 
            else
                dbus_message_iter_get_basic(&args, &sigvalue);
         
            printf("Got Signal with value %s\n", sigvalue);
        }

        // free the message
        dbus_message_unref(msg);
    }
    // close the connection
    dbus_connection_close(conn);
    */
	


	/* send signal
    DBusMessage* 	msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError 		err;
    int 			ret;	
    dbus_uint32_t 	serial = 0;
    char* 			sigvalue = "message input";

    printf("Sending signal with value %s\n", sigvalue);

    // initialise the error value
    dbus_error_init(&err);

    // connect to the DBUS system bus, and check for errors
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Connection Error (%s)\n", err.message); 
        dbus_error_free(&err); 
    }
    if (NULL == conn) { 
        return;
    }

    // register our name on the bus, and check for errors
    ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
    if (dbus_error_is_set(&err)) { 
        fprintf(stderr, "Send - Name Error (%s)\n", err.message); 
        dbus_error_free(&err); 
    }
    if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
        return;
    } 


    // create a signal & check for errors 
    msg = dbus_message_new_signal("/org/freedesktop/Notifications", // object name of the signal
                                  "org.freedesktop.Notifications", // interface name of the signal
                                  "SystemNoteDialog"); // name of the signal
    if (NULL == msg) 
    { 
        fprintf(stderr, "Message Null\n"); 
        return;
    }

    // append arguments onto signal
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &sigvalue)) {
        fprintf(stderr, "Out Of Memory!\n"); 
        return;
    }

    // send the message and flush the connection
    if (!dbus_connection_send(conn, msg, &serial)) {
      fprintf(stderr, "Out Of Memory!\n"); 
      return;
    }
    dbus_connection_flush(conn);
   
    printf("Signal Sent\n");
   
    // free the message and close the connection
    dbus_message_unref(msg);
    dbus_connection_close(conn);
    */
/*
void InstanceCheck::bring_this_instance_forward() const
{
	printf("going forward\n");
	//GUI::wxGetApp().GetTopWindow()->Iconize(false); // restore the window if minimized
	GUI::wxGetApp().GetTopWindow()->SetFocus();  // focus on my window
	GUI::wxGetApp().GetTopWindow()->Raise();  // bring window to front
	GUI::wxGetApp().GetTopWindow()->Show(true); // show the window
}
*/

namespace GUI {

wxDEFINE_EVENT(EVT_LOAD_MODEL_OTHER_INSTANCE, LoadFromOtherInstanceEvent);

void OtherInstanceMessageHandler::init(wxEvtHandler* callback_evt_handler)
{
	assert(!m_initialized);
	assert(m_callback_evt_handler == nullptr);
	if (m_initialized) 
		return;

	m_initialized = true;
	m_callback_evt_handler = callback_evt_handler;

#if _WIN32 
	create_listener_window();
#endif  //_WIN32

#if defined(__APPLE__)
	this->register_for_messages();
#endif //__APPLE__

#ifdef BACKGROUND_MESSAGE_LISTENER
	m_thread = boost::thread((boost::bind(&OtherInstanceMessageHandler::listen, this)));
#endif //BACKGROUND_MESSAGE_LISTENER
}
void OtherInstanceMessageHandler::shutdown()
{
	assert(m_initialized);
	if (m_initialized) {
#if __APPLE__
		//delete macos implementation
		this->unregister_for_messages();
#endif //__APPLE__
#ifdef BACKGROUND_MESSAGE_LISTENER
		if (m_thread.joinable()) {
			// Stop the worker thread, if running.
			{
				// Notify the worker thread to cancel wait on detection polling.
				std::lock_guard<std::mutex> lck(m_thread_stop_mutex);
				m_stop = true;
			}
			m_thread_stop_condition.notify_all();
			// Wait for the worker thread to stop.
			m_thread.join();
			m_stop = false;
		}
#endif //BACKGROUND_MESSAGE_LISTENER
	m_initialized = false;
	}
}
void OtherInstanceMessageHandler::handle_message(const std::string message) {
	std::vector<boost::filesystem::path> paths;
	auto                                 next_space = message.find(' ');
	size_t                               last_space = 0;
	int                                  counter = 0;
	BOOST_LOG_TRIVIAL(error) << "got message " << message;
	while (next_space != std::string::npos)
	{
		const std::string possible_path = message.substr(last_space, next_space - last_space);
		if (counter != 0 && boost::filesystem::exists(possible_path)) {
			paths.emplace_back(boost::filesystem::path(possible_path));
		}
		last_space = next_space;
		next_space = message.find(' ', last_space + 1);
		counter++;
	}
	if (counter != 0 && boost::filesystem::exists(message.substr(last_space + 1))) {
		paths.emplace_back(boost::filesystem::path(message.substr(last_space + 1)));
	}
	if (!paths.empty()) {
		wxEvtHandler* evt_handler = GUI::wxGetApp().plater(); //assert here?
		if (evt_handler) {
			wxPostEvent(evt_handler, GUI::LoadFromOtherInstanceEvent(GUI::EVT_LOAD_MODEL_OTHER_INSTANCE, std::vector<boost::filesystem::path>(std::move(paths))));
		}
	}
}

#ifdef BACKGROUND_MESSAGE_LISTENER
void OtherInstanceMessageHandler::listen()
{
#ifndef  __linux__
	return;
#endif // ! __linux__
	for (;;) {
		// Wait for 1 second 
		// Cancellable.
		{
			std::unique_lock<std::mutex> lck(m_thread_stop_mutex);
			m_thread_stop_condition.wait_for(lck, std::chrono::seconds(1), [this] { return m_stop; });
		}
		if (m_stop)
			// Stop the worker thread.
			break;
		//listen here
		//std::cout<<"listening"<<std::endl;
		//this->handle_message("");
     }
}
#endif BACKGROUND_MESSAGE_LISTENER
} //namespace GUI
} // namespace Slic3r
