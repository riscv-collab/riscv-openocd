/* SPDX-License-Identifier: GPL-2.0-or-later */

/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2007,2008 Øyvind Harboe                                 *
 *   oyvind.harboe@zylin.com                                               *
 ***************************************************************************/

#ifndef OPENOCD_HELPER_COMMAND_H
#define OPENOCD_HELPER_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

#include <helper/jim-nvp.h>
#include <helper/list.h>
#include <helper/types.h>

/* To achieve C99 printf compatibility in MinGW, gnu_printf should be
 * used for __attribute__((format( ... ))), with GCC v4.4 or later
 */
#if (defined(IS_MINGW) && (((__GNUC__ << 16) + __GNUC_MINOR__) >= 0x00040004))
#define PRINTF_ATTRIBUTE_FORMAT gnu_printf
#else
#define PRINTF_ATTRIBUTE_FORMAT printf
#endif

/**
 * OpenOCD command mode is COMMAND_CONFIG at start, then switches to COMMAND_EXEC
 * during the execution of command 'init'.
 * The field 'mode' in struct command_registration specifies in which command mode
 * the command can be executed:
 * - during COMMAND_CONFIG only,
 * - during COMMAND_EXEC only,
 * - in both modes (COMMAND_ANY).
 */
enum command_mode {
	COMMAND_EXEC,
	COMMAND_CONFIG,
	COMMAND_ANY,
	COMMAND_UNKNOWN = -1, /* error condition */
};

struct command_context;

/** The type signature for command context's output handler. */
typedef int (*command_output_handler_t)(struct command_context *context,
		const char *line);

struct command_context {
	Jim_Interp *interp;
	enum command_mode mode;
	struct target *current_target;
		/* The target set by 'targets xx' command or the latest created */
	struct target *current_target_override;
		/* If set overrides current_target
		 * It happens during processing of
		 *	1) a target prefixed command
		 *	2) an event handler
		 * Pay attention to reentrancy when setting override.
		 */
	command_output_handler_t output_handler;
	void *output_handler_priv;
	struct list_head *help_list;
};

struct command;

/**
 * When run_command is called, a new instance will be created on the
 * stack, filled with the proper values, and passed by reference to the
 * required COMMAND_HANDLER routine.
 */
struct command_invocation {
	struct command_context *ctx;
	struct command *current;
	const char *name;
	unsigned argc;
	const char **argv;
	Jim_Obj *output;
};

/**
 * Return true if the command @c cmd is registered by OpenOCD.
 */
bool jimcmd_is_oocd_command(Jim_Cmd *cmd);

/**
 * Return the pointer to the command's private data specified during the
 * registration of command @a cmd .
 */
void *jimcmd_privdata(Jim_Cmd *cmd);

/**
 * Command handlers may be defined with more parameters than the base
 * set provided by command.c.  This macro uses C99 magic to allow
 * defining all such derivative types using this macro.
 */
#define __COMMAND_HANDLER(name, extra ...) \
		int name(struct command_invocation *cmd, ## extra)

/**
 * Use this to macro to call a command helper (or a nested handler).
 * It provides command handler authors protection against reordering or
 * removal of unused parameters.
 *
 * @b Note: This macro uses lexical capture to provide some arguments.
 * As a result, this macro should be used @b only within functions
 * defined by the COMMAND_HANDLER or COMMAND_HELPER macros.  Those
 * macros provide the expected lexical context captured by this macro.
 * Furthermore, it should be used only from the top-level of handler or
 * helper function, or care must be taken to avoid redefining the same
 * variables in intervening scope(s) by accident.
 */
#define CALL_COMMAND_HANDLER(name, extra ...) \
		name(cmd, ## extra)

/**
 * Always use this macro to define new command handler functions.
 * It ensures the parameters are ordered, typed, and named properly, so
 * they be can be used by other macros (e.g. COMMAND_PARSE_NUMBER).
 * All command handler functions must be defined as static in scope.
 */
#define COMMAND_HANDLER(name) \
		static __COMMAND_HANDLER(name)

/**
 * Similar to COMMAND_HANDLER, except some parameters are expected.
 * A helper is globally-scoped because it may be shared between several
 * source files (e.g. the s3c24xx device command helper).
 */
#define COMMAND_HELPER(name, extra ...) __COMMAND_HANDLER(name, extra)

/**
 * Use this macro to access the command being handled,
 * rather than accessing the variable directly.  It may be moved.
 */
#define CMD (cmd)
/**
 * Use this macro to access the context of the command being handled,
 * rather than accessing the variable directly.  It may be moved.
 */
#define CMD_CTX (cmd->ctx)
/**
 * Use this macro to access the number of arguments for the command being
 * handled, rather than accessing the variable directly.  It may be moved.
 */
#define CMD_ARGC (cmd->argc)
/**
 * Use this macro to access the arguments for the command being handled,
 * rather than accessing the variable directly.  It may be moved.
 */
#define CMD_ARGV (cmd->argv)
/**
 * Use this macro to access the name of the command being handled,
 * rather than accessing the variable directly.  It may be moved.
 */
#define CMD_NAME (cmd->name)
/**
 * Use this macro to access the current command being handled,
 * rather than accessing the variable directly.  It may be moved.
 */
#define CMD_CURRENT (cmd->current)
/**
 * Use this macro to access the invoked command handler's data pointer,
 * rather than accessing the variable directly.  It may be moved.
 */
#define CMD_DATA (CMD_CURRENT->jim_handler_data)

/**
 * The type signature for command handling functions.  They are
 * usually registered as part of command_registration, providing
 * a high-level means for executing a command.
 *
 * If the command fails, it *MUST* return a value != ERROR_OK
 * (many commands break this rule, patches welcome!)
 *
 * This is *especially* important for commands such as writing
 * to flash or verifying memory. The reason is that those commands
 * can be used by programs to determine if the operation succeeded
 * or not. If the operation failed, then a program can try
 * an alternative approach.
 *
 * Returning ERROR_COMMAND_SYNTAX_ERROR will have the effect of
 * printing out the syntax of the command.
 */
typedef __COMMAND_HANDLER((*command_handler_t));

struct command {
	char *name;
	command_handler_t handler;
	Jim_CmdProc *jim_handler;
	void *jim_handler_data;
		/* Command handlers can use it for any handler specific data */
	struct target *jim_override_target;
		/* Used only for target of target-prefixed cmd */
	enum command_mode mode;
};

/*
 * Return the struct command pointer kept in private data
 * Used to enforce check on data type
 */
static inline struct command *jim_to_command(Jim_Interp *interp)
{
	return Jim_CmdPrivData(interp);
}

/*
 * Commands should be registered by filling in one or more of these
 * structures and passing them to [un]register_commands().
 *
 * A conventional format should be used for help strings, to provide both
 * usage and basic information:
 * @code
 * "@<options@> ... - some explanation text"
 * @endcode
 *
 * @param name The name of the command to register, which must not have
 * been registered previously in the intended context.
 * @param handler The callback function that will be called.  If NULL,
 * then the command serves as a placeholder for its children or a script.
 * @param mode The command mode(s) in which this command may be run.
 * @param help The help text that will be displayed to the user.
 */
struct command_registration {
	const char *name;
	command_handler_t handler;
	Jim_CmdProc *jim_handler;
	enum command_mode mode;
	const char *help;
	/** a string listing the options and arguments, required or optional */
	const char *usage;

	/**
	 * If non-NULL, the commands in @c chain will be registered in
	 * the same context and scope of this registration record.
	 * This allows modules to inherit lists commands from other
	 * modules.
	 */
	const struct command_registration *chain;
};

/** Use this as the last entry in an array of command_registration records. */
#define COMMAND_REGISTRATION_DONE { .name = NULL, .chain = NULL }

int __register_commands(struct command_context *cmd_ctx, const char *cmd_prefix,
		const struct command_registration *cmds, void *data,
		struct target *override_target);

/**
 * Register one or more commands in the specified context, as children
 * of @c parent (or top-level commends, if NULL).  In a registration's
 * record contains a non-NULL @c chain member and name is NULL, the
 * commands on the chain will be registered in the same context.
 * Otherwise, the chained commands are added as children of the command.
 *
 * @param cmd_ctx The command_context in which to register the command.
 * @param cmd_prefix Register this command as a child of this, or NULL to
 * register a top-level command.
 * @param cmds Pointer to an array of command_registration records that
 * contains the desired command parameters.  The last record must have
 * NULL for all fields.
 * @returns ERROR_OK on success; ERROR_FAIL if any registration fails.
 */
static inline int register_commands(struct command_context *cmd_ctx, const char *cmd_prefix,
		const struct command_registration *cmds)
{
	return __register_commands(cmd_ctx, cmd_prefix, cmds, NULL, NULL);
}

/**
 * Register one or more commands, as register_commands(), plus specify
 * that command should override the current target
 *
 * @param cmd_ctx The command_context in which to register the command.
 * @param cmd_prefix Register this command as a child of this, or NULL to
 * register a top-level command.
 * @param cmds Pointer to an array of command_registration records that
 * contains the desired command parameters.  The last record must have
 * NULL for all fields.
 * @param target The target that has to override current target.
 * @returns ERROR_OK on success; ERROR_FAIL if any registration fails.
 */
static inline int register_commands_override_target(struct command_context *cmd_ctx,
		const char *cmd_prefix, const struct command_registration *cmds,
		struct target *target)
{
	return __register_commands(cmd_ctx, cmd_prefix, cmds, NULL, target);
}

/**
 * Register one or more commands, as register_commands(), plus specify
 * a pointer to command private data that would be accessible through
 * the macro CMD_DATA. The private data will not be freed when command
 * is unregistered.
 *
 * @param cmd_ctx The command_context in which to register the command.
 * @param cmd_prefix Register this command as a child of this, or NULL to
 * register a top-level command.
 * @param cmds Pointer to an array of command_registration records that
 * contains the desired command parameters.  The last record must have
 * NULL for all fields.
 * @param data The command private data.
 * @returns ERROR_OK on success; ERROR_FAIL if any registration fails.
 */
static inline int register_commands_with_data(struct command_context *cmd_ctx,
		const char *cmd_prefix, const struct command_registration *cmds,
		void *data)
{
	return __register_commands(cmd_ctx, cmd_prefix, cmds, data, NULL);
}

/**
 * Unregisters all commands from the specified context.
 * @param cmd_ctx The context that will be cleared of registered commands.
 * @param cmd_prefix If given, only clear commands from under this one command.
 * @returns ERROR_OK on success, or an error code.
 */
int unregister_all_commands(struct command_context *cmd_ctx,
		const char *cmd_prefix);

/**
 * Unregisters the help for all commands. Used at exit to remove the help
 * added through the commands 'add_help_text' and 'add_usage_text'.
 * @param cmd_ctx The context that will be cleared of registered helps.
 * @returns ERROR_OK on success, or an error code.
 */
int help_del_all_commands(struct command_context *cmd_ctx);

void command_set_output_handler(struct command_context *context,
		command_output_handler_t output_handler, void *priv);


int command_context_mode(struct command_context *context, enum command_mode mode);

/* Return the current command context associated with the Jim interpreter or
 * alternatively the global default command interpreter
 */
struct command_context *current_command_context(Jim_Interp *interp);
/**
 * Creates a new command context using the startup TCL provided and
 * the existing Jim interpreter, if any. If interp == NULL, then command_init
 * creates a command interpreter.
 */
struct command_context *command_init(const char *startup_tcl, Jim_Interp *interp);
/**
 * Shutdown a command context.
 *
 * Free the command context and the associated Jim interpreter.
 *
 * @param context The command_context that will be destroyed.
 */
void command_exit(struct command_context *context);
/**
 * Creates a copy of an existing command context.  This does not create
 * a deep copy of the command list, so modifications in one context will
 * affect all shared contexts.  The caller must track reference counting
 * and ensure the commands are freed before destroying the last instance.
 * @param cmd_ctx The command_context that will be copied.
 * @returns A new command_context with the same state as the original.
 */
struct command_context *copy_command_context(struct command_context *cmd_ctx);
/**
 * Frees the resources associated with a command context.  The commands
 * are not removed, so unregister_all_commands() must be called first.
 * @param context The command_context that will be destroyed.
 */
void command_done(struct command_context *context);

/*
 * command_print() and command_print_sameline() are used to produce the TCL
 * output of OpenOCD commands. command_print() automatically adds a '\n' at
 * the end or the format string. Use command_print_sameline() to avoid the
 * trailing '\n', e.g. to concatenate the command output in the same line.
 * The very last '\n' of the command is stripped away (see run_command()).
 * For commands that strictly require a '\n' as last output character, add
 * it explicitly with either an empty command_print() or with a '\n' in the
 * last command_print() and add a comment to document it.
 */
void command_print(struct command_invocation *cmd, const char *format, ...)
__attribute__ ((format (PRINTF_ATTRIBUTE_FORMAT, 2, 3)));
void command_print_sameline(struct command_invocation *cmd, const char *format, ...)
__attribute__ ((format (PRINTF_ATTRIBUTE_FORMAT, 2, 3)));

int command_run_line(struct command_context *context, char *line);
int command_run_linef(struct command_context *context, const char *format, ...)
__attribute__ ((format (PRINTF_ATTRIBUTE_FORMAT, 2, 3)));
void command_output_text(struct command_context *context, const char *data);

void process_jim_events(struct command_context *cmd_ctx);

#define ERROR_COMMAND_CLOSE_CONNECTION		(-600)
#define ERROR_COMMAND_SYNTAX_ERROR			(-601)
#define ERROR_COMMAND_NOTFOUND				(-602)
#define ERROR_COMMAND_ARGUMENT_INVALID		(-603)
#define ERROR_COMMAND_ARGUMENT_OVERFLOW		(-604)
#define ERROR_COMMAND_ARGUMENT_UNDERFLOW	(-605)

int parse_ulong(const char *str, unsigned long *ul);
int parse_ullong(const char *str, unsigned long long *ul);

int parse_long(const char *str, long *ul);
int parse_llong(const char *str, long long *ul);

#define DECLARE_PARSE_WRAPPER(name, type) \
		int parse ## name(const char *str, type * ul)

DECLARE_PARSE_WRAPPER(_uint, unsigned);
DECLARE_PARSE_WRAPPER(_u64, uint64_t);
DECLARE_PARSE_WRAPPER(_u32, uint32_t);
DECLARE_PARSE_WRAPPER(_u16, uint16_t);
DECLARE_PARSE_WRAPPER(_u8, uint8_t);

DECLARE_PARSE_WRAPPER(_int, int);
DECLARE_PARSE_WRAPPER(_s64, int64_t);
DECLARE_PARSE_WRAPPER(_s32, int32_t);
DECLARE_PARSE_WRAPPER(_s16, int16_t);
DECLARE_PARSE_WRAPPER(_s8, int8_t);

DECLARE_PARSE_WRAPPER(_target_addr, target_addr_t);

/**
 * @brief parses the string @a in into @a out as a @a type, or prints
 * a command error and passes the error code to the caller.  If an error
 * does occur, the calling function will return the error code produced
 * by the parsing function (one of ERROR_COMMAND_ARGUMENT_*).
 *
 * This function may cause the calling function to return immediately,
 * so it should be used carefully to avoid leaking resources.  In most
 * situations, parsing should be completed in full before proceeding
 * to allocate resources, and this strategy will most prevents leaks.
 */
#define COMMAND_PARSE_NUMBER(type, in, out) \
	do { \
		int retval_macro_tmp = parse_ ## type(in, &(out)); \
		if (retval_macro_tmp != ERROR_OK) { \
			command_print(CMD, stringify(out) \
				" option value ('%s') is not valid", in); \
			return retval_macro_tmp; \
		} \
	} while (0)

#define COMMAND_PARSE_ADDRESS(in, out) \
	COMMAND_PARSE_NUMBER(target_addr, in, out)

/**
 * @brief parses the command argument at position @a argn into @a out
 * as a @a type, or prints a command error referring to @a name_str
 * and passes the error code to the caller. @a argn will be incremented
 * if no error occurred. Otherwise the calling function will return
 * the error code produced by the parsing function.
 *
 * This function may cause the calling function to return immediately,
 * so it should be used carefully to avoid leaking resources.  In most
 * situations, parsing should be completed in full before proceeding
 * to allocate resources, and this strategy will most prevents leaks.
 */
#define COMMAND_PARSE_ADDITIONAL_NUMBER(type, argn, out, name_str) \
	do { \
		if (argn+1 >= CMD_ARGC || CMD_ARGV[argn+1][0] == '-') { \
			command_print(CMD, "no " name_str " given"); \
			return ERROR_FAIL; \
		} \
		++argn; \
		COMMAND_PARSE_NUMBER(type, CMD_ARGV[argn], out); \
	} while (0)

/**
 * @brief parses the command argument at position @a argn into @a out
 * as a @a type if the argument @a argn does not start with '-'.
 * and passes the error code to the caller. @a argn will be incremented
 * if no error occurred. Otherwise the calling function will return
 * the error code produced by the parsing function.
 *
 * This function may cause the calling function to return immediately,
 * so it should be used carefully to avoid leaking resources.  In most
 * situations, parsing should be completed in full before proceeding
 * to allocate resources, and this strategy will most prevents leaks.
 */
#define COMMAND_PARSE_OPTIONAL_NUMBER(type, argn, out) \
	do { \
		if (argn+1 < CMD_ARGC && CMD_ARGV[argn+1][0] != '-') { \
			++argn; \
			COMMAND_PARSE_NUMBER(type, CMD_ARGV[argn], out); \
		} \
	} while (0)

/**
 * Parse the string @c as a binary parameter, storing the boolean value
 * in @c out.  The strings @c on and @c off are used to match different
 * strings for true and false options (e.g. "on" and "off" or
 * "enable" and "disable").
 */
#define COMMAND_PARSE_BOOL(in, out, on, off) \
	do { \
		bool value; \
		int retval_macro_tmp = command_parse_bool_arg(in, &value); \
		if (retval_macro_tmp != ERROR_OK) { \
			command_print(CMD, stringify(out) \
				" option value ('%s') is not valid", in); \
			command_print(CMD, "  choices are '%s' or '%s'", \
				on, off); \
			return retval_macro_tmp; \
		} \
		out = value; \
	} while (0)

int command_parse_bool_arg(const char *in, bool *out);
COMMAND_HELPER(handle_command_parse_bool, bool *out, const char *label);

/** parses an on/off command argument */
#define COMMAND_PARSE_ON_OFF(in, out) \
	COMMAND_PARSE_BOOL(in, out, "on", "off")
/** parses an enable/disable command argument */
#define COMMAND_PARSE_ENABLE(in, out) \
	COMMAND_PARSE_BOOL(in, out, "enable", "disable")

#endif /* OPENOCD_HELPER_COMMAND_H */
