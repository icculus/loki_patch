
/* Create a recursive patch between the two trees of files */
extern int tree_patch(const char *o_top, const char *o_path,
                      const char *n_top, const char *n_path, loki_patch *patch);
extern int tree_add_file(const char *path, const char *dst, loki_patch *patch);
extern int tree_add_path(const char *path, const char *dst, loki_patch *patch);
extern int tree_patch_file(const char *o_path, const char *n_path,
                           const char *dst, loki_patch *patch);
extern int tree_symlink_file(const char *link, const char *dst, loki_patch *patch);
extern int tree_del_path(const char *dst, loki_patch *patch);
extern int tree_del_file(const char *dst, loki_patch *patch);
