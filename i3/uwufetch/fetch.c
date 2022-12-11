/*
 *  UwUfetch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef __APPLE__
	#include <TargetConditionals.h> // for checking iOS
#endif
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__BSD__)
	#include <sys/sysctl.h>
	#if defined(__OPENBSD__)
		#include <sys/time.h>
	#else
		#include <time.h>
	#endif // defined(__OPENBSD__)
#else	   // defined(__APPLE__) || defined(__BSD__)
	#ifdef __BSD__
	#else // defined(__BSD__) || defined(_WIN32)
		#ifndef _WIN32
			#ifndef __OPENBSD__
				#include <sys/sysinfo.h>
			#else  // __OPENBSD__
			#endif // __OPENBSD__
		#else	   // _WIN32
			#include <sysinfoapi.h>
		#endif // _WIN32
	#endif	   // defined(__BSD__) || defined(_WIN32)
#endif		   // defined(__APPLE__) || defined(__BSD__)
#ifndef _WIN32
	#include <sys/ioctl.h>
	#include <sys/utsname.h>
#else // _WIN32
	#include <windows.h>
CONSOLE_SCREEN_BUFFER_INFO csbi;
#endif // _WIN32

#define LIBFETCH_INTERNAL // to do certain things only when included from the library itself
#include "fetch.h"

#ifdef __APPLE__
// buffers where data fetched from sysctl are stored
// CPU
	#define CPUBUFFERLEN 128

char cpu_buffer[CPUBUFFERLEN];
size_t cpu_buffer_len = CPUBUFFERLEN;

// Installed RAM
int64_t mem_buffer	  = 0;
size_t mem_buffer_len = sizeof(mem_buffer);

// uptime
struct timeval time_buffer;
size_t time_buffer_len = sizeof(time_buffer);
#endif // __APPLE__

struct package_manager {
	char command_string[128]; // command to get number of packages installed
	char pkgman_name[16];	  // name of the package manager
};

#ifdef __APPLE__
// gets the uptime for mac os
int uptime_apple() {
	int mib[2] = {CTL_KERN, KERN_BOOTTIME};
	sysctl(mib, 2, &time_buffer, &time_buffer_len, NULL, 0);

	time_t bsec = time_buffer.tv_sec;
	time_t csec = time(NULL);

	return difftime(csec, bsec);
}
#endif

#ifdef __BSD__
// gets the uptime for freebsd
int uptime_freebsd() { // this code is from coreutils uptime: https://github.com/coreutils/coreutils/blob/master/src/uptime.c
	int boot_time		  = 0;
	static int request[2] = {CTL_KERN, KERN_BOOTTIME};
	struct timeval result;
	size_t result_len = sizeof result;

	if (sysctl(request, 2, &result, &result_len, NULL, 0) >= 0)
		boot_time = result.tv_sec;
	int time_now = time(NULL);
	return time_now - boot_time;
}
#endif

// truncates the given string
void truncate_str(char* string, int target_width) {
	char arr[target_width];
	for (int i = 0; i < target_width; i++) arr[i] = string[i];
	string = arr;
}

// remove square brackets (for gpu names)
void remove_brackets(char* str) {
	int i = 0, j = 0;
	while (i < (int)strlen(str))
		if (str[i] == '[' || str[i] == ']')
			for (j = i; j < (int)strlen(str); j++) str[j] = str[j + 1];
		else
			i++;
}

// tries to get memory usage
void get_ram(char* buffer, int buf_sz, struct info* user_info) {
#ifndef __APPLE__
	#ifdef _WIN32
	FILE* mem_used_fp	   = popen("wmic os get freevirtualmemory", "r");	   // free memory
	FILE* mem_total_fp	   = popen("wmic os get totalvirtualmemorysize", "r"); // total memory
	char mem_used_ch[2137] = {0}, mem_total_ch[2137] = {0};

	while (fgets(mem_total_ch, sizeof(mem_total_ch), mem_total_fp) != NULL) {
		if (strstr(mem_total_ch, "TotalVirtualMemorySize") != 0)
			continue;
		else if (strstr(mem_total_ch, "  ") == 0)
			continue;
		else
			user_info->ram_total = atoi(mem_total_ch) / 1024;
	}
	while (fgets(mem_used_ch, sizeof(mem_used_ch), mem_used_fp) != NULL) {
		if (strstr(mem_used_ch, "FreeVirtualMemory") != 0)
			continue;
		else if (strstr(mem_used_ch, "  ") == 0)
			continue;
		else
			user_info->ram_used =
				user_info->ram_total - (atoi(mem_used_ch) / 1024);
	}
	pclose(mem_used_fp);
	pclose(mem_total_fp);
	#else // if not _WIN32
	FILE* meminfo;

		#ifdef __BSD__
			#ifndef __OPENBSD__
	meminfo = popen("LANG=EN_us freecolor -om 2> /dev/null", "r"); // free alternative for freebsd
			#else
	meminfo = popen("LANG=EN_us vmstat 2> /dev/null | grep -v 'procs' | grep -v 'r' | awk '{print $3 \" / \" $4}'", "r"); // free alternative for openbsd
			#endif
		#else
	// getting memory info from /proc/meminfo: https://github.com/KittyKatt/screenFetch/issues/386#issuecomment-249312716
	meminfo = fopen("/proc/meminfo", "r"); // popen("LANG=EN_us free -m 2> /dev/null", "r"); // get ram info with free
		#endif
	// brackets are here to restrict the access to this int variables, which are temporary
	{
		#ifndef __OPENBSD__
		int memtotal = 0, shmem = 0, memfree = 0, buffers = 0, cached = 0, sreclaimable = 0;
		#endif
		while (fgets(buffer, buf_sz, meminfo)) {
		#ifndef __OPENBSD__
			sscanf(buffer, "MemTotal:       %d", &memtotal);
			sscanf(buffer, "Shmem:             %d", &shmem);
			sscanf(buffer, "MemFree:        %d", &memfree);
			sscanf(buffer, "Buffers:          %d", &buffers);
			sscanf(buffer, "Cached:          %d", &cached);
			sscanf(buffer, "SReclaimable:     %d", &sreclaimable);
		#else
			sscanf(buffer, "%dM / %dM", &user_info->ram_used, &user_info->ram_total);
		#endif
		}
		#ifndef __OPENBSD__
		user_info->ram_total = memtotal / 1024;
		user_info->ram_used	 = ((memtotal + shmem) - (memfree + buffers + cached + sreclaimable)) / 1024;
		#endif
	}

	// while (fgets(buffer, buf_sz, meminfo)) // old way to get ram usage that uses the "free" command
	// 	// free command prints like this: "Mem:" total     used    free shared    buff/cache      available
	// 	sscanf(buffer, "Mem: %d %d", &user_info->ram_total, &user_info->ram_used);
	fclose(meminfo);
	#endif
#else // if __APPLE__
	  // Used
	FILE *mem_wired_fp,
		*mem_active_fp, *mem_compressed_fp;
	mem_wired_fp	  = popen("vm_stat | awk '/wired/ { printf $4 }' | cut -d '.' -f 1", "r");
	mem_active_fp	  = popen("vm_stat | awk '/active/ { printf $3 }' | cut -d '.' -f 1", "r");
	mem_compressed_fp = popen("vm_stat | awk '/occupied/ { printf $5 }' | cut -d '.' -f 1", "r");
	char mem_wired_ch[2137], mem_active_ch[2137], mem_compressed_ch[2137];
	while (fgets(mem_wired_ch, sizeof(mem_wired_ch), mem_wired_fp) != NULL)
		while (fgets(mem_active_ch, sizeof(mem_active_ch), mem_active_fp) != NULL)
			while (fgets(mem_compressed_ch, sizeof(mem_compressed_ch), mem_compressed_fp) != NULL)
				;

	pclose(mem_wired_fp);
	pclose(mem_active_fp);
	pclose(mem_compressed_fp);

	int mem_wired	   = atoi(mem_wired_ch);
	int mem_active	   = atoi(mem_active_ch);
	int mem_compressed = atoi(mem_compressed_ch);

	// Total
	sysctlbyname("hw.memsize", &mem_buffer, &mem_buffer_len, NULL, 0);
	user_info->ram_used	 = ((mem_wired + mem_active + mem_compressed) * 4 / 1024);
	user_info->ram_total = mem_buffer / 1024 / 1024;
#endif
}

// tries to get installed gpu(s)
void get_gpu(char* buffer, int buf_sz, struct info* user_info) {
	int gpuc = 0; // gpu counter
#ifndef _WIN32
	setenv("LANG", "en_US", 1); // force language to english
#endif
	FILE* gpu;
#ifndef _WIN32
	gpu = popen("lshw -class display 2> /dev/null", "r");

	// add all gpus to the array gpu_model
	while (fgets(buffer, buf_sz, gpu))
		if (sscanf(buffer, "    product: %[^\n]", user_info->gpu_model[gpuc]))
			gpuc++;
#endif

	if (strlen(user_info->gpu_model[0]) < 2) {
		// get gpus with lspci command
		if (strcmp(user_info->os_name, "android") != 0) {
#ifndef __APPLE__
	#ifdef _WIN32
			gpu = popen("wmic PATH Win32_VideoController GET Name", "r");
	#else
			gpu = popen("lspci -mm 2> /dev/null | grep \"VGA\" | awk -F '\"' '{print $4 $5 $6}'", "r");
	#endif
#else
			gpu = popen("system_profiler SPDisplaysDataType | awk -F ': ' '/Chipset Model: /{ print $2 }'", "r");
#endif
		} else
			gpu = popen("getprop ro.hardware.vulkan 2> /dev/null", "r"); // for android
	}

	// get all the gpus
	while (fgets(buffer, buf_sz, gpu)) {
		// windows
		if (strstr(buffer, "Name") || (strlen(buffer) == 2))
			continue;
		else if (sscanf(buffer, "%[^\n]", user_info->gpu_model[gpuc]))
			gpuc++;
	}
	fclose(gpu);

	// format gpu names
	for (int i = 0; i < gpuc; i++) {
		remove_brackets(user_info->gpu_model[i]);
		truncate_str(user_info->gpu_model[i], user_info->target_width);
	}
}

// tries to get screen resolution
void get_res(char* buffer, int buf_sz, struct info* user_info) {
#ifndef _WIN32
	FILE* resolution = popen("xwininfo -root 2> /dev/null | grep -E 'Width|Height'", "r");
	while (fgets(buffer, buf_sz, resolution)) {
		sscanf(buffer, "  Width: %d", &user_info->screen_width);
		sscanf(buffer, "  Height: %d", &user_info->screen_height);
	}
#else
	// TODO: get resolution on windows
#endif
}

// tries to get the installed package count and package managers name
void get_pkg(struct info* user_info) { // this is just a function that returns the total of installed packages
	user_info->pkgs = 0;
#ifndef __APPLE__
	// this function is not used on mac os because it causes lots of problems
	#ifndef _WIN32
	// all supported package managers
	struct package_manager pkgmans[] =
		{{"apt list --installed 2> /dev/null | wc -l", "(apt)"},
		 {"apk info 2> /dev/null | wc -l", "(apk)"},
		 //  {"dnf list installed 2> /dev/null | wc -l", "(dnf)"}, // according to https://stackoverflow.com/questions/48570019/advantages-of-dnf-vs-rpm-on-fedora, dnf and rpm return the same number of packages
		 {"qlist -I 2> /dev/null | wc -l", "(emerge)"},
		 {"flatpak list 2> /dev/null | wc -l", "(flatpak)"},
		 {"snap list 2> /dev/null | wc -l", "(snap)"},
		 {"guix package --list-installed 2> /dev/null | wc -l", "(guix)"},
		 {"nix-store -q --requisites /run/current-system/sw 2> /dev/null | wc -l", "(nix)"},
		 {"pacman -Qq 2> /dev/null | wc -l", "(pacman)"},
		 {"pkg info 2>/dev/null | wc -l", "(pkg)"},
		 {"pkg_info 2>/dev/null | wc -l | sed \"s/ //g\"", "(pkg)"},
		 {"port installed 2> /dev/null | tail -n +2 | wc -l", "(port)"},
		 {"brew list 2> /dev/null | wc -l", "(brew)"},
		 {"rpm -qa --last 2> /dev/null | wc -l", "(rpm)"},
		 {"xbps-query -l 2> /dev/null | wc -l", "(xbps)"},
		 {"zypper -q se --installed-only 2> /dev/null | wc -l", "(zypper)"}};
	const int pkgman_count = sizeof(pkgmans) / sizeof(pkgmans[0]); // number of package managers
	int comma_separator	   = 0;
	for (int i = 0; i < pkgman_count; i++) {
		struct package_manager* current = &pkgmans[i]; // pointer to current package manager

		FILE* fp			   = popen(current->command_string, "r"); // trying current package manager
		unsigned int pkg_count = 0;

		if (fscanf(fp, "%u", &pkg_count) == 3) continue; // if a number is found, continue the loop
		pclose(fp);

		// adding a package manager with its package count to user_info->pkgman_name
		user_info->pkgs += pkg_count;
		if (pkg_count > 0) {
			if (comma_separator++) strcat(user_info->pkgman_name, ", ");
			char spkg_count[16];
			sprintf(spkg_count, "%u", pkg_count);
			strcat(user_info->pkgman_name, spkg_count);
			strcat(user_info->pkgman_name, " ");
			strcat(user_info->pkgman_name, current->pkgman_name);
		}
	}
	#else  // _WIN32
	// chocolatey for windows
	FILE* fp = popen("choco list -l --no-color 2> nul", "r");
	unsigned int pkg_count;
	char buffer[7562] = {0};
	while (fgets(buffer, sizeof(buffer), fp)) {
		sscanf(buffer, "%u packages installed.", &pkg_count);
	}
	if (fp) pclose(fp);

	user_info->pkgs = pkg_count;
	char spkg_count[16];
	sprintf(spkg_count, "%u", pkg_count);
	strcat(user_info->pkgman_name, spkg_count);
	strcat(user_info->pkgman_name, " ");
	strcat(user_info->pkgman_name, "(chocolatey)");
	#endif // _WIN32
#endif
}

// Retrieves system information
struct info get_info() {
	struct info user_info = {0};
	int buf_sz			  = 256;
	char buffer[buf_sz]; // line buffer

	// get terminal width used to truncate long names
#ifndef _WIN32
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &user_info.win);
	user_info.target_width = user_info.win.ws_col - 30;
#else  // _WIN32
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	user_info.ws_col  = csbi.srWindow.Right - csbi.srWindow.Left - 29;
	user_info.ws_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#endif // _WIN32

	// os version, cpu and board info
#ifdef __OPENBSD__
	FILE* os_release = popen("echo ID=openbsd", "r"); // os-release does not exist in OpenBSD
#else
	FILE* os_release  = fopen("/etc/os-release", "r"); // os name file
#endif
#ifndef __BSD__
	FILE* cpuinfo = fopen("/proc/cpuinfo", "r"); // cpu name file for not-freebsd systems
#else
	FILE* cpuinfo	  = popen("sysctl hw.model", "r"); // cpu name command for freebsd
#endif
	// trying to get some kind of information about the name of the computer (hopefully a product full name)
	FILE* model_fp /* = fopen("/sys/devices/virtual/dmi/id/product_version", "r") */; // trying to get product version
	// if (!model_fp) model_fp = fopen("/sys/devices/virtual/dmi/id/product_name", "r"); // trying to get product name
	// if (!model_fp) model_fp = fopen("/sys/devices/virtual/dmi/id/board_name", "r");	  // trying to get motherboard name
	char model_filename[3][256] = {"/sys/devices/virtual/dmi/id/product_version",
								   "/sys/devices/virtual/dmi/id/product_name",
								   "/sys/devices/virtual/dmi/id/board_name"};

	char tmp_model[3][256]; // temporary variable to store the contents of all 3 files
	int longest_model = 0, best_len = 0, currentlen = 0;
	for (int i = 0; i < 3; i++) {
		// read file
		model_fp = fopen(model_filename[i], "r");
		if (model_fp) {
			fgets(tmp_model[i], 256, model_fp);
			tmp_model[i][strlen(tmp_model[i]) - 1] = '\0';
			fclose(model_fp);
		}
		// choose the file with the longest name
		currentlen = strlen(tmp_model[i]);
		if (currentlen > best_len) {
			best_len	  = currentlen;
			longest_model = i;
		}
	}
	sprintf(user_info.model, "%s", tmp_model[longest_model]); // read model name
#ifdef _WIN32
	// all the previous files obviously did not exist on windows
	model_fp = popen("wmic computersystem get model", "r");
	while (fgets(buffer, sizeof(buffer), model_fp)) {
		if (strstr(buffer, "Model") != 0)
			continue;
		else {
			sprintf(user_info.model, "%s", buffer);
			user_info.model[strlen(user_info.model) - 2] = '\0';
			break;
		}
	}
#elif defined(__BSD__) || defined(__APPLE__)
	#if defined(__BSD__) && !defined(__OPENBSD__)
		#define HOSTCTL "hw.hv_vendor"
	#elif defined(__APPLE__)
		#define HOSTCTL "hw.model"
	#elif defined(__OPENBSD__)
		#define HOSTCTL "hw.product"
	#endif
	model_fp		  = popen("sysctl " HOSTCTL, "r");
	while (fgets(buffer, sizeof(buffer), model_fp))
		if (sscanf(buffer, HOSTCTL
	#ifndef __OPENBSD__
				   ": "
	#else
				   "="
	#endif
				   "%[^\n]",
				   user_info.model))
			break;
#endif				  // _WIN32
	if (os_release) { // get normal vars if os_release exists
		while (fgets(buffer, sizeof(buffer), os_release) && !(sscanf(buffer, "\nID=\"%s\"", user_info.os_name) || sscanf(buffer, "\nID=%s", user_info.os_name)))
			;
		// sometimes for some reason sscanf reads the last '\"' too
		int os_name_len = strlen(user_info.os_name);
		if (user_info.os_name[os_name_len - 1] == '\"') {
			user_info.os_name[os_name_len - 1] = '\0';
		}
		/* trying to detect amogos because in its os-release file ID value is just "debian",
			 will be removed when amogos will have an os-release file with ID=amogos */
		if (strcmp(user_info.os_name, "debian") == 0 || strcmp(user_info.os_name, "raspbian") == 0) {
			DIR* amogos_plymouth = opendir("/usr/share/plymouth/themes/amogos");
			if (amogos_plymouth) {
				closedir(amogos_plymouth);
				sprintf(user_info.os_name, "amogos");
			}
		}
		// getting cpu name
		while (fgets(buffer, sizeof(buffer), cpuinfo)) {
#ifdef __BSD__
			if (sscanf(buffer, "hw.model"
	#ifdef __FREEBSD__
							   ": "
	#elif defined(__OPENBSD__)
							   "="
	#endif
							   "%[^\n]",
					   user_info.cpu_model))
				break;
#else
			if (sscanf(buffer, "model name    : %[^\n]", user_info.cpu_model))
				break;
#endif // __BSD__
		}
		// getting username
		char* tmp_user = getenv("USER");
		if (tmp_user == NULL)
			sprintf(user_info.user, "%s", "");
		else
			sprintf(user_info.user, "%s", tmp_user);
		fclose(os_release);
	} else { // try for android vars, next for Apple var, or unknown system
		// android
		DIR* system_app		 = opendir("/system/app/");
		DIR* system_priv_app = opendir("/system/priv-app/");
		DIR* library		 = opendir("/Library/");
		if (system_app && system_priv_app) {
			closedir(system_app);
			closedir(system_priv_app);
			sprintf(user_info.os_name, "android");
			// username
			FILE* whoami = popen("whoami", "r");
			if (fscanf(whoami, "%s", user_info.user) == 3)
				sprintf(user_info.user, "unknown");
			pclose(whoami);
			// model name
			model_fp = popen("getprop ro.product.model", "r");
			while (fgets(buffer, sizeof(buffer), model_fp) && !sscanf(buffer, "%[^\n]", user_info.model))
				;
#ifndef __BSD__
			while (fgets(buffer, sizeof(buffer), cpuinfo) && !sscanf(buffer, "Hardware        : %[^\n]", user_info.cpu_model))
				;
#endif
		} else if (library) { // Apple
			closedir(library);
#ifdef __APPLE__
			sysctlbyname("machdep.cpu.brand_string", &cpu_buffer, &cpu_buffer_len, NULL, 0); // cpu name
	#ifndef __IPHONE__
			sprintf(user_info.os_name, "macos");
	#else
			sprintf(user_info.os_name, "ios");
	#endif
			sprintf(user_info.cpu_model, "%s", cpu_buffer);
#endif
		} else
			sprintf(user_info.os_name, "unknown"); // if no option before is working, the system is unknown
	}
#ifndef __BSD__
	fclose(cpuinfo);
#endif
#ifndef _WIN32
	gethostname(user_info.host, 256);  // hostname
	char* tmp_shell = getenv("SHELL"); // shell name
	if (tmp_shell == NULL)
		sprintf(user_info.shell, "%s", "");
	else
		sprintf(user_info.shell, "%s", tmp_shell);
	#ifdef __linux__
	if (strlen(user_info.shell) > 16) // android shell name was too long
		memmove(&user_info.shell, &user_info.shell[27], strlen(user_info.shell));
	#endif
#else  // if _WIN32
	// cpu name
	cpuinfo = popen("wmic cpu get caption", "r");
	while (fgets(buffer, sizeof(buffer), cpuinfo)) {
		if (strstr(buffer, "Caption") != 0)
			continue;
		else {
			sprintf(user_info.cpu_model, "%s", buffer);
			user_info.cpu_model[strlen(user_info.cpu_model) - 2] = '\0';
			break;
		}
	}
	// username
	FILE* user_host_fp = popen("wmic computersystem get username", "r");
	while (fgets(buffer, sizeof(buffer), user_host_fp)) {
		if (strstr(buffer, "UserName") != 0)
			continue;
		else {
			sscanf(buffer, "%[^\\]%s", user_info.host, user_info.user);
			memmove(user_info.user, user_info.user + 1,
					sizeof(user_info.user) - 1);
			break;
		}
	}
	// powershell version
	FILE* shell_fp = popen("powershell $PSVersionTable", "r");
	sprintf(user_info.shell, "PowerShell ");
	char tmp_shell[64];
	while (fgets(buffer, sizeof(buffer), shell_fp) && sscanf(buffer, "PSVersion                      %s", tmp_shell) == 0)
		;
	strcat(user_info.shell, tmp_shell);
#endif // _WIN32
	truncate_str(user_info.cpu_model, user_info.target_width);

	// system resources
#ifndef _WIN32
	uname(&user_info.sys_var);
#endif // _WIN32
#ifndef __APPLE__
	#ifndef __BSD__
		#ifndef _WIN32
	sysinfo(&user_info.sys); // somehow this function has to be called again in print_info()
		#else
	GetSystemInfo(&user_info.sys);
		#endif
	#endif
#endif

#ifndef _WIN32
	truncate_str(user_info.sys_var.release, user_info.target_width);
	sprintf(user_info.kernel, "%s %s %s", user_info.sys_var.sysname, user_info.sys_var.release, user_info.sys_var.machine); // kernel name
	truncate_str(user_info.kernel, user_info.target_width);
#else  // _WIN32
	// os name and windows version
	sprintf(user_info.os_name, "windows");
	FILE* kernel_fp = popen("wmic computersystem get systemtype", "r");
	while (fgets(buffer, sizeof(buffer), kernel_fp)) {
		if (strstr(buffer, "SystemType") != 0)
			continue;
		else {
			sprintf(user_info.kernel, "%s", buffer);
			user_info.kernel[strlen(user_info.kernel) - 2] = '\0';
			break;
		}
	}
	if (kernel_fp) pclose(kernel_fp);
#endif // _WIN32
	get_ram(buffer, buf_sz, &user_info);
	get_gpu(buffer, buf_sz, &user_info);
	get_res(buffer, buf_sz, &user_info);
	get_pkg(&user_info);
	return user_info;
}
