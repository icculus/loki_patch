/* Functions to load and free the patch */
extern loki_patch *load_patch(const char *patchfile);
extern void free_add_path(struct op_add_path *add_path_list);
extern void free_add_file(struct op_add_file *add_file_list);
extern void free_patch_file(struct op_patch_file *patch_file_list);
extern void free_symlink_file(struct op_symlink_file *symlink_file_list);
extern void free_del_file(struct op_del_file *del_file_list);
extern void free_del_path(struct op_del_path *del_path_list);
extern void free_patch(loki_patch *patch);
