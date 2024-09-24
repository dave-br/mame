#ifdef __cplusplus
extern "C" {
#endif

void * __cdecl mame_mdi_simp_open_new(const char * file_path);
unsigned short __cdecl mame_mdi_simp_add_source_file_path(void * mdi_simp_state, const char * source_file_path);
void __cdecl mame_mdi_simp_add_line_mapping(void * mdi_simp_state, unsigned short address_first, unsigned short address_last, unsigned short source_file_index, unsigned int line_number);
void __cdecl mame_mdi_simp_add_symbol(void * mdi_simp_state, const char * symbol_name, int symbol_value);
void __cdecl mame_mdi_simp_close(void * mdi_simp_state);

#ifdef __cplusplus
}
#endif