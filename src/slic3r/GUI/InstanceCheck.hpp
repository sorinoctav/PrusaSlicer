#ifndef slic3r_InstanceCheck_hpp_
#define slic3r_InstanceCheck_hpp_

#if _WIN32
#include <windows.h>
#endif

#include <string>

#include <boost/filesystem.hpp>

#include <boost/thread.hpp>
#include <tbb/mutex.h>
#include <condition_variable>

// Custom wxWidget events
#include "Event.hpp"

namespace Slic3r {

bool instance_check(int argc, char** argv);

#if __APPLE__
void send_message_mac(const std::string msg);
#endif //__APPLE__

namespace GUI {

#if __linux__
    #define BACKGROUND_MESSAGE_LISTENER
#endif // __linux__

using LoadFromOtherInstanceEvent = Event<std::vector<boost::filesystem::path>>;
wxDECLARE_EVENT(EVT_LOAD_MODEL_OTHER_INSTANCE, LoadFromOtherInstanceEvent);

class OtherInstanceMessageHandler
{
public:
	OtherInstanceMessageHandler() = default;
	OtherInstanceMessageHandler(OtherInstanceMessageHandler const&) = delete;
	void operator=(OtherInstanceMessageHandler const&) = delete;
	~OtherInstanceMessageHandler() { assert(!m_initialized); }

	//inits listening, on each platform different. On linux starts background thread
	void init(wxEvtHandler* callback_evt_handler);
	// stops listening, on linux stops the background thread
	void shutdown();

	//finds paths to models in message(= command line arguments, first should be prusaSlicer executable) and sends them to plater via LoadFromOtherInstanceEvent
	void handle_message(const std::string message);

private:
	bool                    m_initialized { false };
	wxEvtHandler*           m_callback_evt_handler { nullptr };

#ifdef BACKGROUND_MESSAGE_LISTENER
	//worker thread to listen incoming dbus communication
	boost::thread 			m_thread;
	std::condition_variable m_thread_stop_condition;
	mutable std::mutex 		m_thread_stop_mutex;
	bool 					m_stop{ false };
	bool					m_start{ true };
	
	// background thread method
	void listen();
#endif //BACKGROUND_MESSAGE_LISTENER

#if __APPLE__
	//implemented at InstanceCheckMac.mm
	void register_for_messages();
	void unregister_for_messages();
	// Opaque pointer to RemovableDriveManagerMM
	void* m_impl_osx;
#endif //__APPLE__

};
} // namespace GUI
} // namespace Slic3r
#endif // slic3r_InstanceCheck_hpp_
