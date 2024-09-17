#ifdef __cplusplus
extern "C" {
#endif

void * mame_mdi_simp_open_new(const char * file_path);
void mame_mdi_simp_add_line_mapping(void * mdi_simp_state, unsigned short address_first, unsigned short address_last, unsigned int line_number);
void mame_mdi_simp_close(void * mdi_simp_state);

#ifdef __cplusplus
}
#endif