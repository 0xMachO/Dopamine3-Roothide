// const char *const envp[] so both char** (mutated copies) and const char**
// (original argv/envp) callers convert implicitly without qualifier warnings.
int envbuf_len(const char *const envp[]);
char **envbuf_mutcopy(const char *const envp[]);
void envbuf_free(char *envp[]);
int envbuf_find(const char *const envp[], const char *name);
const char *envbuf_getenv(const char *const envp[], const char *name);
void envbuf_setenv(char **envpp[], const char *name, const char *value);
void envbuf_unsetenv(char **envpp[], const char *name);