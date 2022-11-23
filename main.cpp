#include "DOA6/RdbFile.h"
#include "debug.h"

bool get_hex_name(const std::string &fn, uint32_t *ret)
{
	if (!Utils::BeginsWith(fn, "0x", false))
		return false;
	
	if (fn.length() == 2)
		return false;
	
	for (size_t i = 2; i < fn.length(); i++)
	{
		char ch = tolower(fn[i]);
		
		if (ch == '.')
			break;
		
		bool ok = false;
		
		if (ch >= '0' && ch <= '9')
			ok = true;
		else if (ch >= 'a' && ch <= 'f')
			ok = true;		
		
		if (!ok)
			return false;
	}
	
	*ret = Utils::GetUnsigned(fn);
	return true;
}

int extract_menu(RdbFile &rdb, const std::string &rdb_path)
{
	UPRINTF("Now select the desired extract option\n");
	UPRINTF("1) Extract all contents\n");
	UPRINTF("2) Extract contents by type\n");
	UPRINTF("3) Extract specific file\n");
	
	char ch;
	do
	{
		ch = getchar();
	} while (ch != '1' && ch != '2' && ch != '3');	
	fseek(stdin, 0, SEEK_END);
	
	std::string name = Utils::GetFileNameString(rdb_path);
	std::string rdb_dir;
	std::string extract_dir;
				
	size_t dot = name.rfind('.');
	if (dot == std::string::npos)
		extract_dir = name + "_extracted";
	else
		extract_dir = name.substr(0, dot);
	
	
	size_t slash = rdb_path.find('/'); 
	if (slash == std::string::npos)
		rdb_dir = "./";
	else
		rdb_dir = Utils::GetDirNameString(rdb_path);
	
	extract_dir = Utils::MakePathString(rdb_dir, extract_dir);
	Utils::Mkdir(extract_dir);
	
	extract_dir.push_back('/');
	
	if (ch == '1' || ch == '2')
	{
		uint32_t filter_type = 0;
		
		if (ch == '2')
		{
			char str[40];
			std::string filter_ext;
			
			UPRINTF("Specify the extension of the content you want to extract (.g1m, .g1t, etc): ");
			gets(str);
			
			filter_ext = str;
			Utils::TrimString(filter_ext);
			
			if (filter_ext.length() == 0)
			{
				DPRINTF("Error: no type was specified.\n");
				return -1;
			}
			
			filter_type = rdb.GetTypeByExtension(filter_ext);
			if (filter_type == 0)
			{
				DPRINTF("\"%s\" is not a valid DOA6 file type.\n", filter_ext.c_str());
				return -1;
			}
			
			UPRINTF("Extracting...\n");
		}
		
		size_t num_extract = 0;		
		
		for (size_t i = 0; i < rdb.GetNumFiles(); i++)
		{
			if (ch == '1')
			{
				UPRINTF("Extracting %Id/%Id\n", i+1, rdb.GetNumFiles());
			}
			else
			{
				if (!rdb.MatchesType(i, filter_type))
					continue;
			}
			
			if (!rdb.ExtractFile(i, extract_dir))
				DPRINTF("(Skipped entry %Id)\n", i);
			else
				num_extract++;
		}
		
		if (ch == '1')
			UPRINTF("All files extracted.\n");
		else
			UPRINTF("%Id files extracted.\n", num_extract);
	}
	else if (ch == '3')
	{
		char str[100];
		
		UPRINTF("Specify the filename or the filename hash of the file to extract.\n");
		gets(str);
		
		std::string file_name = str;
		Utils::TrimString(file_name);
		
		uint32_t id;
		size_t index;
		
		if (get_hex_name(file_name, &id))
		{
			index = rdb.FindFileByID(id);
			if (index == (size_t)-1)
			{
				DPRINTF("Failed to find a file with filename hash of 0x%08x\n", id);
				return -1;
			}
		}
		else
		{
			index = rdb.FindFileByName(file_name);
			if (index == (size_t)-1)
			{
				DPRINTF("Failed to find a file with that filename.\n");
				return -1;
			}
		}
		
		if (rdb_dir.back() != '/') // No need to check for \ because the path was converted to use / 
			rdb_dir.push_back('/');
		
		if (rdb.ExtractFile(index, rdb_dir))
		{
			UPRINTF("File succesfully extracted.\n");
		}
		else
		{
			DPRINTF("Failed to extract file.\n");
		}		
	}
	
	return 0;
}

bool reimport_visitor(const std::string &path, bool, void *custom_param)
{
	std::vector<std::string> *pfiles_list = (std::vector<std::string> *)custom_param;
	pfiles_list->push_back(path);
	
	return true;
}

int reimport_menu(RdbFile &rdb, const std::string &rdb_path)
{
	static bool write_bin_done = false;
	
	if (!write_bin_done)
	{
		std::string name = Utils::GetFileNameString(rdb_path);
		std::string rdb_dir;
		std::string wb_path;
			
		size_t slash = rdb_path.find('/'); 
		if (slash == std::string::npos)
			rdb_dir = "./";
		else
			rdb_dir = Utils::GetDirNameString(rdb_path);
		
		
		name += ".bin_9";
		wb_path = Utils::MakePathString(rdb_dir, name);
		
		if (!Utils::FileExists(wb_path))
		{
			if (!Utils::WriteFileBool(wb_path, nullptr, 0))
            {
				DPRINTF("Failed to create file \"%s\" You may not have write permissions on this directory.\n", wb_path.c_str());
				return -1;
			}
		}
		
		if (!rdb.SetWriteBin(wb_path, -1, 9))
		{
			DPRINTF("Failed to set the .bin for write.\n");
			return -1;
		}
		
		write_bin_done = true;
	}
	
	static char str[MAX_PATH];	
	std::string path;
	std::vector<std::string> files_list;
	
	UPRINTF("Drag here the file to reimport and press enter. You can also drag a directory if you want to reimport multiple files.\n");
	gets(str);
	
	path = str;
	Utils::TrimString(path);
	
	if (path.length() > 0)
	{
		if (path[0] == '"')
			path = path.substr(1);
		
		if (path.length() > 0 && path.back() == '"')
			path.pop_back();
	}
	
	if (Utils::DirExists(path))
	{
		Utils::VisitDirectory(path, true, false, false, reimport_visitor, &files_list);
	}
	else if (Utils::FileExists(path))
	{
		files_list.push_back(path);
	}
	else
	{
		DPRINTF("Specified file or folder doesn't exist or some error happened.\n");
		return -1;
	}
	
	if (files_list.size() == 0)
	{
		DPRINTF("That directory is empty. No files to reimport.\n");
		return -1;
	}
	
	size_t num_reimported = 0;
	
	for (const std::string &file : files_list)
	{
		std::string file_name = Utils::GetFileNameString(file);
		
		UPRINTF("Reimporting %s...\n", file.c_str());
		
		uint32_t id;
		size_t index;
		
		if (get_hex_name(file_name, &id))
		{
			index = rdb.FindFileByID(id);
			if (index == (size_t)-1)
			{
				DPRINTF("(Skipping this file) Failed to find a file with filename hash of 0x%08x\n", id);
				continue;
			}
		}
		else
		{
			index = rdb.FindFileByName(file_name);
			if (index == (size_t)-1)
			{
				DPRINTF("(Skipping this file) Failed to find a file with filename of \"%s\"\n", file_name.c_str());
				continue;
			}
		}
		
		//DPRINTF("Index is %Id\n", index);
		
		FileStream in("rb");
		
		if (!in.LoadFromFile(file))
		{
			DPRINTF("(Skipping this file) Failed to open this file: \"%s\"", file.c_str());
			continue;
		}
		
		if (!rdb.ReimportFile(index, &in))
		{
			DPRINTF("Failed to reimport this file\n");
			continue;
		}
		
		num_reimported++;
	}
	
	if (num_reimported > 0)
	{
		if (!rdb.SaveToFile(rdb_path))
		{
			DPRINTF("Failed to resave .rdb\n");
			return -1;
		}
	}
	
	if (files_list.size() == 1)
	{
		if (num_reimported > 0)
		{
			UPRINTF("File succesfully reimported.\n");
		}
		else
		{
			UPRINTF("File wasn't reimported.\n");
		}
	}
	else
	{
		UPRINTF("%Id files were reimported.\n", num_reimported);
	}
	
	return 0;
}

int rdb_menu(const std::string &rdb_path)
{
	RdbFile rdb(rdb_path);
	
	UPRINTF("Loading rdb file... ");
	
	if (!rdb.LoadFromFile(rdb_path))
	{
		DPRINTF("Load failed.\n");
		return -1;
	}
	
	UPRINTF("done (rdb has %Id files)\n", rdb.GetNumFiles());
	
	while (1)
	{	
		UPRINTF("Select an option (enter number and press enter)\n");
		UPRINTF("Press Ctrl-C whenever you want to quit the program.\n");
		UPRINTF("1) Extract file(s)\n");
		UPRINTF("2) Reimport file(s)\n");
		
		char ch;
		
		do
		{
			ch = getchar();		
		} while (ch != '1' && ch != '2');
		fseek(stdin, 0, SEEK_END);
		
		if (ch == '1')
		{
			extract_menu(rdb, rdb_path);
		}
		else if (ch == '2')
		{
			reimport_menu(rdb, rdb_path);
		}
		
		UPRINTF("Press enter to go back to main menu. Ctrl-C to quit the program\n");
		getchar();
	}
	
	return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        DPRINTF("Bad usage. Usage: %s file\n", argv[0]);
		UPRINTF("Press enter to exit.");
		getchar();
        return -1;
    }

    int ret = rdb_menu(Utils::NormalizePath(argv[1]));

	fseek(stdin, 0, SEEK_END);
	UPRINTF("Press enter to exit.");
    getchar();

    return ret;
}
