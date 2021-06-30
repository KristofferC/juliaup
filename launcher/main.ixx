module;

#include <string>
#include <string_view>
#include <codecvt>
#include <iostream>
#include <ranges>
#include <fstream>
#include <filesystem>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>
#include "winrt/Windows.Web.Http.h"
#include "winrt/Windows.Storage.Streams.h"
#include "../json/single_include/nlohmann/json.hpp"
#include <windows.h>
#include "version.h"

export module main;

import Tokenizer;

using namespace winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::Storage;
using json = nlohmann::json;

std::string GetLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

/*++

Routine Description:

	This routine appends the given argument to a command line such
	that CommandLineToArgvW will return the argument string unchanged.
	Arguments in a command line should be separated by spaces; this
	function does not add these spaces.

Arguments:

	Argument - Supplies the argument to encode.

	CommandLine - Supplies the command line to which we append the encoded argument string.

	Force - Supplies an indication of whether we should quote
			the argument even if it does not contain any characters that would
			ordinarily require quoting.

Return Value:

	None.

Environment:

	Arbitrary.

This function was copied from https://web.archive.org/web/20190109172835/https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
on 6/7/2021 by David Anthoff.
--*/
void ArgvQuote(
	const std::wstring& Argument,
	std::wstring& CommandLine,
	bool Force
)
{
	//
	// Unless we're told otherwise, don't quote unless we actually
	// need to do so --- hopefully avoid problems if programs won't
	// parse quotes properly
	//

	if (Force == false &&
		Argument.empty() == false &&
		Argument.find_first_of(L" \t\n\v\"") == Argument.npos)
	{
		CommandLine.append(Argument);
	}
	else {
		CommandLine.push_back(L'"');

		for (auto It = Argument.begin(); ; ++It) {
			unsigned NumberBackslashes = 0;

			while (It != Argument.end() && *It == L'\\') {
				++It;
				++NumberBackslashes;
			}

			if (It == Argument.end()) {

				//
				// Escape all backslashes, but let the terminating
				// double quotation mark we add below be interpreted
				// as a metacharacter.
				//

				CommandLine.append(NumberBackslashes * 2, L'\\');
				break;
			}
			else if (*It == L'"') {

				//
				// Escape all backslashes and the following
				// double quotation mark.
				//

				CommandLine.append(NumberBackslashes * 2 + 1, L'\\');
				CommandLine.push_back(*It);
			}
			else {

				//
				// Backslashes aren't special here.
				//

				CommandLine.append(NumberBackslashes, L'\\');
				CommandLine.push_back(*It);
			}
		}

		CommandLine.push_back(L'"');
	}
}

HRESULT StartProcess(LPCWSTR applicationName, LPWSTR commandLine, LPCWSTR currentDirectory, DWORD timeout)
{
	STARTUPINFO info;
	GetStartupInfo(&info);

	PROCESS_INFORMATION processInfo{};

	BOOL ret = CreateProcessW(
		applicationName,
		commandLine, //commandLine,
		nullptr, nullptr, // Process/ThreadAttributes
		true, // InheritHandles
		0, //EXTENDED_STARTUPINFO_PRESENT, // CreationFlags
		nullptr, // Environment
		currentDirectory, //currentDirectory,
		//(LPSTARTUPINFO)&startupInfoEx,
		&info,
		&processInfo);

	if (!ret) {
		auto error_message = GetLastErrorAsString();

		printf(error_message.c_str());

		return ERROR;
	}

	// RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), processInfo.hProcess == INVALID_HANDLE_VALUE);
	DWORD waitResult = ::WaitForSingleObject(processInfo.hProcess, timeout);
	// RETURN_LAST_ERROR_IF_MSG(waitResult != WAIT_OBJECT_0, "Waiting operation failed unexpectedly.");
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);

	return ERROR_SUCCESS;
}

extern "C" IMAGE_DOS_HEADER __ImageBase;

std::filesystem::path GetExecutablePath()
{
	std::wstring buffer;
	size_t nextBufferLength = MAX_PATH;

	for (;;)
	{
		buffer.resize(nextBufferLength);
		nextBufferLength *= 2;

		SetLastError(ERROR_SUCCESS);

		auto pathLength = GetModuleFileName(reinterpret_cast<HMODULE>(&__ImageBase), &buffer[0], static_cast<DWORD>(buffer.length()));

		if (pathLength == 0)
			throw std::exception("GetModuleFileName failed"); // You can call GetLastError() to get more info here

		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			buffer.resize(pathLength);
			return buffer;
		}
	}
}

std::wstring getCurrentPlatform() {
#ifdef _M_X64
	return std::wstring{ L"x64" };
#endif

#ifdef _M_IX86
	return std::wstring{ L"x86" };
#endif
}

std::filesystem::path getJuliaupPath() {
	std::filesystem::path homedirPath = std::wstring{ Windows::Storage::UserDataPaths::GetDefault().Profile() };
	return homedirPath / ".julia" / "juliaup";
}

void initial_setup() {
	auto juliaupFolder = getJuliaupPath();

	if (!std::filesystem::exists(juliaupFolder / "juliaup.json")) {

		std::filesystem::path myOwnPath = GetExecutablePath();

		auto pathOfBundledJulia = myOwnPath.parent_path().parent_path() / "BundledJulia";

		std::wstring bundledVersion{ winrt::to_hstring(JULIA_APP_BUNDLED_JULIA) };

		auto platform = getCurrentPlatform();

		auto targetPath = juliaupFolder / platform / (L"julia-" + bundledVersion);

		std::filesystem::create_directories(targetPath);

		std::filesystem::copy(pathOfBundledJulia, targetPath, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);

		json j;
		j["Default"] = "release";
		j["InstalledVersions"] = {
			{
				winrt::to_string(bundledVersion + L"~" + platform),
				{
					{"Path", winrt::to_string(std::wstring{std::filesystem::path{ L"." } / platform / (L"julia-" + bundledVersion)})}
				}
			}
		};
		j["InstalledChannels"] = {
			{
				winrt::to_string(L"release"),
				{
					{"Version", winrt::to_string(bundledVersion + L"~" + platform)}
				}
			}
		};

		std::ofstream o(juliaupFolder / "juliaup.json");
		o << std::setw(4) << j << std::endl;
	}
}

winrt::fire_and_forget DownloadVersionDBAsync()
{
	co_await winrt::resume_background();

	Windows::Foundation::Uri uri{ L"https://www.david-anthoff.com/juliaup-versionsdb-winnt-" + getCurrentPlatform() + L".json" };

	std::filesystem::path juliaupFolderPath{ std::filesystem::path {std::wstring{ Windows::Storage::UserDataPaths::GetDefault().Profile() } } / ".julia" / "juliaup" };

	Windows::Web::Http::HttpClient httpClient{};

	// Always catch network exceptions for async methods
	try
	{

		auto response{ co_await httpClient.GetAsync(uri) };

		auto buffer{ co_await response.Content().ReadAsBufferAsync() };

		auto folder{ co_await Windows::Storage::StorageFolder::GetFolderFromPathAsync(std::wstring{juliaupFolderPath}) };

		auto file{ co_await folder.CreateFileAsync(L"juliaup-versionsdb-winnt-" + getCurrentPlatform() + L".json", Windows::Storage::CreationCollisionOption::ReplaceExisting) };

		co_await Windows::Storage::FileIO::WriteBufferAsync(file, buffer);
	}
	catch (winrt::hresult_error const& ex)
	{
		// Details in ex.message() and ex.to_abi().
	}
}

std::filesystem::path getJuliaupconfigPath()
{
	return getJuliaupPath() / "juliaup.json";
}

json loadVersionDB()
{
	auto currentPlatform{ getCurrentPlatform() };
	std::filesystem::path versionDBFilename{ std::wstring {L"juliaup-versionsdb-winnt-"} + currentPlatform + L".json" };

	std::vector<std::filesystem::path> version_db_search_paths{
		getJuliaupPath() / versionDBFilename,
		GetExecutablePath().parent_path().parent_path() / L"VersionsDB" / versionDBFilename
	};

	for (auto& i : version_db_search_paths) {
		if (std::filesystem::exists(i)) {
			std::ifstream file(i);

			json versiondbData;

			file >> versiondbData;

			return versiondbData;
		}
	}

	// TODO Throw exception
}

json loadConfigDB()
{
	auto configFilePath{ getJuliaupconfigPath() };

	if (std::filesystem::exists(configFilePath)) {
		std::ifstream i(configFilePath);
		json configFile;

		i >> configFile;

		return configFile;
	}
	else
	{
		std::wcout << "ERROR: Could not read the juliaup configuration file." << std::endl;

		// TODO Throw an exception
		return 1;
	}
}

void isChannelUptodate(std::wstring channelAsWstring, json configDB, json versionsDB)
{
	std::string channel{ winrt::to_string(channelAsWstring) };
	auto latestVersion{ versionsDB["AvailableChannels"][channel]["Version"].get<std::string>() };
	auto currentVersion{ configDB["InstalledChannels"][channel]["Version"].get<std::string>() };

	if (latestVersion != currentVersion) {
		std::cout << "The latest version of Julia in the `" << channel << "` channel is " << latestVersion << ". You currently have " << currentVersion << " installed. Run:" << std::endl;
		std::cout << std::endl;
		std::cout << "  juliaup update" << std::endl;
		std::cout << std::endl;
		std::cout << "to install Julia " << latestVersion << " and update the `" << channel << "` channel to that version." << std::endl;
	}
}

// Copied from StackOverflow
template <typename charType>
void ReplaceSubstring(std::basic_string<charType>& subject,
	const std::basic_string<charType>& search,
	const std::basic_string<charType>& replace)
{
	if (search.empty()) { return; }
	typename std::basic_string<charType>::size_type pos = 0;
	while ((pos = subject.find(search, pos)) != std::basic_string<charType>::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
}

export int main(int argc, char* argv[])
{
	init_apartment();

	SetConsoleTitle(L"Julia");

	json versionsDB{ loadVersionDB() };

	//DownloadVersionDBAsync();

	initial_setup();

	bool juliaVersionFromCmdLine = false;

	json configDB{ loadConfigDB() };

	std::wstring juliaChannelToUse{ winrt::to_hstring(configDB["/Default"_json_pointer]) };

	std::wstring exeArgString = std::wstring{ L"" };

	for (int i = 1; i < argc; i++) {
		std::wstring curr = std::wstring{ winrt::to_hstring(argv[i]) };

		exeArgString.append(L" ");

		if (i==1 && curr._Starts_with(L"+")) {
			juliaChannelToUse = curr.substr(1);
			juliaVersionFromCmdLine = true;
		}
		else {
			ArgvQuote(curr, exeArgString, false);
		}
	}

	auto escapedJuliaChannelToUse = juliaChannelToUse;
	ReplaceSubstring(escapedJuliaChannelToUse, std::wstring{ L"~" }, std::wstring{ L"~0" });

	std::filesystem::path julia_path;

	if (configDB["InstalledChannels"][winrt::to_string(juliaChannelToUse)].contains("Command")) {
		julia_path = std::wstring{ winrt::to_hstring(configDB["InstalledChannels"][winrt::to_string(juliaChannelToUse)]["Command"].get<std::string>()) };
	}
	else if (configDB["InstalledChannels"][winrt::to_string(juliaChannelToUse)].contains("Version")) {
		json::json_pointer jsonPathToChannelDetail{ winrt::to_string(L"/InstalledChannels/" + escapedJuliaChannelToUse + L"/Version") };
		std::wstring JuliaVersionToUse = std::wstring{ winrt::to_hstring(configDB[jsonPathToChannelDetail]) };

		auto escapedJuliaVersionToUse = JuliaVersionToUse;
		ReplaceSubstring(escapedJuliaVersionToUse, std::wstring{ L"~" }, std::wstring{ L"~0" });

		json::json_pointer jsonPathToVersionDetail{ winrt::to_string(L"/InstalledVersions/" + escapedJuliaVersionToUse + L"/Path") };
		std::filesystem::path relativeVersionPath{ std::wstring {winrt::to_hstring(configDB[jsonPathToVersionDetail]) } };
		julia_path = std::filesystem::canonical(getJuliaupPath() / relativeVersionPath / L"bin" / L"julia.exe");

		isChannelUptodate(juliaChannelToUse, configDB, versionsDB);
	}

	//std::filesystem::path currentDirectory = L"";

	exeArgString.insert(0, julia_path);

	HRESULT hr = StartProcess(NULL, exeArgString.data(), nullptr, INFINITE);
	if (hr != ERROR_SUCCESS)
	{
		printf("Error return from launching process.");
	}

	return 0;
}
