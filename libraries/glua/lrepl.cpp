#define lrepl_cpp

#include <glua/lprefix.h>
#include <glua/lauxlib.h>
#include <glua/lobject.h>
#include <glua/lapi.h>
#include <glua/lua_api.h>
#include <glua/lua_lib.h>
#include <glua/glua_lutil.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <utility>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <string.h>
#include <signal.h>
#include <sstream>
#include <iostream>

static lua_State *globalL = nullptr;
static const char *progname = "thinkyoung_lua";

/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void ll_message(lua_State *L, const char *pname, const char *msg) {
    if (pname) luaL_writestringerror(L, "%s: ", pname);
    luaL_writestringerror(L, "%s\n", msg);
}

/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop(lua_State *L, lua_Debug *ar) {
    (void)ar;  /* unused arg. */
    lua_sethook(L, nullptr, 0, 0);  /* reset hook */
    luaL_error(L, "interrupted!");
}

/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Lua state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == nullptr) {  /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
            lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
            return 1;  /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)",
            luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

/*
** Interface to 'lua_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall(lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, msghandler);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    globalL = L;  /* to be available to 'laction' */
    signal(SIGINT, laction);  /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    if (lua_gettop(L) > 0)
        lua_remove(L, base);  /* remove message handler from the stack */
    return status;
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report(lua_State *L, int status) {
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        ll_message(L, progname, msg);
        lua_pop(L, 1);  /* remove message */
    }
    return status;
}

static int dochunk(lua_State *L, int status) {
    if (status == LUA_OK) status = docall(L, 0, 0);
    return report(L, status);
}

static int dofile(lua_State *L, const char *name) {
    return dochunk(L, luaL_loadfile(L, name));
}


static int dostring(lua_State *L, const char *s, const char *name) {
    return dochunk(L, luaL_loadbuffer(L, s, strlen(s), name));
}


/*
** Calls 'require(name)' and stores the result in a global variable
** with the given name.
*/
static int dolibrary(lua_State *L, const char *name) {
    int status;
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    status = docall(L, 1, 1);  /* call 'require(name)' */
    if (status == LUA_OK)
        lua_setglobal(L, name);  /* global[name] = require return */
    return report(L, status);
}

/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs(lua_State *L) {
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    n = (int)luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -i);  /* remove table from the stack */
    return n;
}



/* bits of various argument indicators in 'args' */
#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */

#if !defined(LUA_PROMPT)
#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#endif

#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME		"lua"
#endif

#if !defined(LUA_MAXINPUT)
#define LUA_MAXINPUT		512
#endif

#if !defined(LUA_INIT_VAR)
#define LUA_INIT_VAR		"LUA_INIT"
#endif

#define LUA_INITVARVERSION  \
	LUA_INIT_VAR "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR

/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any Lua code (or an error code if it finds
** any invalid argument). 'first' returns the first not-handled argument
** (either the script name or a bad argument in case of error).
*/
static int collectargs(char **argv, int *first) {
    int args = 0;
    int i;
    for (i = 1; argv[i] != nullptr; i++) {
        *first = i;
        if (argv[i][0] != '-')  /* not an option? */
            return args;  /* stop handling options */
        switch (argv[i][1]) {  /* else check option */
        case '-':  /* '--' */
            if (argv[i][2] != '\0')  /* extra characters after '--'? */
                return has_error;  /* invalid option */
            *first = i + 1;
            return args;
        case '\0':  /* '-' */
            return args;  /* script "name" is '-' */
        case 'E':
            if (argv[i][2] != '\0')  /* extra characters after 1st? */
                return has_error;  /* invalid option */
            args |= has_E;
            break;
        case 'i':
            args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
        case 'v':
            if (argv[i][2] != '\0')  /* extra characters after 1st? */
                return has_error;  /* invalid option */
            args |= has_v;
            break;
        case 'e':
            args |= has_e;  /* FALLTHROUGH */
        case 'l':  /* both options need an argument */
            if (argv[i][2] == '\0') {  /* no concatenated argument? */
                i++;  /* try next 'argv' */
                if (argv[i] == nullptr || argv[i][0] == '-')
                    return has_error;  /* no next argument or it is another option */
            }
            break;
        default:  /* invalid option */
            return has_error;
        }
    }
    *first = i;  /* no script name */
    return args;
}


/*
** Processes options 'e' and 'l', which involve running Lua code.
** Returns 0 if some code raises an error.
*/
static int runargs(lua_State *L, char **argv, int n) {
    int i;
    for (i = 1; i < n; i++) {
        int option = argv[i][1];
        lua_assert(argv[i][0] == '-');  /* already checked */
        if (option == 'e' || option == 'l') {
            int status;
            const char *extra = argv[i] + 2;  /* both options need an argument */
            if (*extra == '\0') extra = argv[++i];
            lua_assert(extra != nullptr);
            status = (option == 'e')
                ? dostring(L, extra, "=(command line)")
                : dolibrary(L, extra);
            if (status != LUA_OK) return 0;
        }
    }
    return 1;
}

static int handle_script(lua_State *L, char **argv) {
    // READ THIS
    int status;
    const char *fname = argv[0];
    if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
        fname = nullptr;  /* stdin */
    status = luaL_loadfile(L, fname);
    if (status == LUA_OK) {
        int n = pushargs(L);  /* push arguments to script */
        status = docall(L, n, LUA_MULTRET);
    }
    return report(L, status);
}

/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print(lua_State *L) {
    int n = lua_gettop(L);
    if (n > 0) {  /* any result to be printed? */
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK)
            ll_message(L, progname, lua_pushfstring(L, "error calling 'print' (%s)",
            lua_tostring(L, -1)));
    }
}


/*
** lua_readline defines how to show a prompt and then read a line from
** the standard input.
** lua_saveline defines how to "save" a read line in a "history".
** lua_freeline defines how to free a line read by lua_readline.
*/
#if !defined(lua_readline)	/* { */

#if defined(LUA_USE_READLINE)	/* { */

#include <readline/readline.h>
#include <readline/history.h>
#define lua_readline(L,b,p)	((void)L, ((b)=readline(p)) != nullptr)
#define lua_saveline(L,line)	((void)L, add_history(line))
#define lua_freeline(L,b)	((void)L, free(b))

#else				/* }{ */

#define lua_readline(L,b,p) \
        ((void)L, fputs(p, (L)->out), fflush((L)->out),  /* show prompt */ \
        fgets(b, LUA_MAXINPUT, (L)->in) != nullptr)  /* get line */
#define lua_saveline(L,line)	{ (void)L; (void)line; }
#define lua_freeline(L,b)	{ (void)L; (void)b; }

#endif				/* } */

#endif				/* } */

/*
** Returns the string to be used as a prompt by the interpreter.
*/
static const char *get_prompt(lua_State *L, int firstline) {
    const char *p;
    lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
    p = lua_tostring(L, -1);
    if (p == nullptr) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
    return p;
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete(lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;  /* else... */
}

/*
** Prompt the user, read a line, and push it into the Lua stack.
*/
static int pushline(lua_State *L, int firstline) {
    char buffer[LUA_MAXINPUT];
    char *b = buffer;
    size_t l;
    const char *prmt = get_prompt(L, firstline);
    int readstatus = lua_readline(L, b, prmt);
    if (readstatus == 0 || lua_gettop(L) < 1)
        return 0;  /* no input (prompt will be popped by caller) */
    lua_pop(L, 1);  /* remove prompt */
    l = strlen(b);
    if (l > 0 && b[l - 1] == '\n')  /* line ends with newline? */
        b[--l] = '\0';  /* remove it */
    if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
        lua_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
    else
        lua_pushlstring(L, b, l);
    lua_freeline(L, b);
    return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn(lua_State *L) {
    const char *line = lua_tostring(L, -1);  /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    if (status == LUA_OK) {
        lua_remove(L, -2);  /* remove modified line */
        if (line[0] != '\0')  /* non empty? */
            lua_saveline(L, line);  /* keep history */
    }
    else
        lua_pop(L, 2);  /* pop result from 'luaL_loadbuffer' and modified line */
    return status;
}


/*
** Read multiple lines until a complete Lua statement
*/
static int multiline(lua_State *L) {
    for (;;) {  /* repeat until gets a complete statement */
        size_t len;
        const char *line = lua_tolstring(L, 1, &len);  /* get what it has */
        int status = luaL_loadbuffer(L, line, len, "=stdin");  /* try it */
        if (!incomplete(L, status) || !pushline(L, 0)) {
            lua_saveline(L, line);  /* keep history */
            return status;  /* cannot or should not try to add continuation line */
        }
        lua_pushliteral(L, "\n");  /* add newline... */
        lua_insert(L, -2);  /* ...between the two lines */
        lua_concat(L, 3);  /* join them */
    }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline(lua_State *L) {
    int status;
    lua_settop(L, 0);
    if (!pushline(L, 1))
        return -1;  /* no input */
    if ((status = addreturn(L)) != LUA_OK)  /* 'return ...' did not work? */
        status = multiline(L);  /* try as command, maybe with continuation lines */
    lua_remove(L, 1);  /* remove line from the stack */
    lua_assert(lua_gettop(L) == 1);
    return status;
}

/*
** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
** print any results.
*/
void luaL_doREPL(lua_State *L)
{
    int status;
    const char *oldprogname = progname;
    progname = nullptr;  /* no 'progname' on errors in interactive mode */
    int *state = lvm::lua::lib::get_repl_state(L);
    while ((status = loadline(L)) != -1 && *state > 0) {
        if (status == LUA_OK)
            status = docall(L, 0, LUA_MULTRET);
        if (status == LUA_OK) l_print(L);
        else report(L, status);
        if (*state <= 0)
            break;
    }
    lua_settop(L, 0);  /* clear stack */
    luaL_writeline(L);
    progname = oldprogname;
}
