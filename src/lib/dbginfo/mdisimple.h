struct mame_debug_info_header
{
	char magic[4];
	char type[4];
	unsigned char version;
};

struct mame_debug_info_simple : mame_debug_info_header
{

};


/*
	mame_debug_info_simple format:
	mame_debug_info_header
*/