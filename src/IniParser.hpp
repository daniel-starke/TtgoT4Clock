/**
 * @file IniParser.hpp
 * @author Daniel Starke
 * @date 2024-04-21
 * @version 2024-05-05
 *
 * Copyright (c) 2024 Daniel Starke
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _INI_PARSER_HPP_
#define _INI_PARSER_HPP_
#include <new>
extern "C" {
#include <ctype.h>
#include <stdint.h>
#include <string.h>
}


/* forward declaration */
template <size_t MaxId, size_t N>
class IniParserSized;


/**
 * Streaming INI parser.
 * The mapping provider is being used to obtain the parsed values.
 *
 * Example:
 * ```cpp
 * char strExample[16] = { 0 };
 * uint32_t numExample = 0;
 * const auto mapping = [&] (IniParser::Context & ctx) -> bool {
 * 	if (ctx.group == "EXAMPLE") {
 * 		if (ctx.key == "STRING") {
 * 			ctx.mapString(strExample);
 * 		} else if (ctx.key == "NUMBER") {
 * 			ctx.mapNum(numExample);
 * 		}
 * 	}
 * 	return true;
 * };
 * IniParser ini(mapping);
 * int ch;
 * do {
 * 	ch = Serial.read();
 * 	ini.parse(ch);
 * } while (ch >= 0);
 * ```
 */
class IniParser {
	template <size_t MaxId, size_t N>
	friend class IniParserSized;
public:
	struct detail {
		/* Template type helpers to avoid dependency on STL. */
		/* false_type */
		struct false_type { static constexpr bool value = false; };
		/* true_type */
		struct true_type { static constexpr bool value = true; };
		/* enable_if */
		template <bool B, typename T = void> struct enable_if {};
		template <typename T> struct enable_if<true, T> { typedef T type; };
		/* is_same */
		template <typename, typename> struct is_same : false_type {};
		template <typename T> struct is_same<T, T> : true_type {};
		/* is_lvalue_reference */
		template <typename T> struct is_lvalue_reference : false_type {};
		template <typename T> struct is_lvalue_reference<T &> : true_type {};
		/* remove_const */
		template <typename T> struct remove_const { typedef T type; };
		template <typename T> struct remove_const<const T> { typedef T type; };
		/* remove_volatile */
		template <typename T> struct remove_volatile { typedef T type; };
		template <typename T> struct remove_volatile<volatile T> { typedef T type; };
		/* remove_cv */
		template <typename T> struct remove_cv { typedef typename remove_const<typename remove_volatile<T>::type>::type type; };
		/* remove_reference */
		template <typename T> struct remove_reference { typedef T type; };
		template <typename T> struct remove_reference<T &> { typedef T type; };
		template <typename T> struct remove_reference<T &&> { typedef T type; };
		/* is_function (simplified) */
		template <typename> struct is_function : false_type {};
		template <typename R, typename ...Args> struct is_function<R(Args...)> : true_type {};
		template <typename R, typename ...Args> struct is_function<R(Args...) const> : true_type {};
		template <typename R, typename ...Args> struct is_function<R(Args...) volatile> : true_type {};
		template <typename R, typename ...Args> struct is_function<R(Args...) volatile const> : true_type {};
		/* decay_function (simplified version of `decay`) */
		template <typename T>
		class decay_function {
		private:
			template <typename U, bool Function> struct DecayImpl { typedef typename remove_cv<U>::type type; };
			template <typename U> struct DecayImpl<U, true> { typedef U * type; };
			typedef typename remove_reference<T>::type NoneRefT;
		public:
			typedef typename DecayImpl<NoneRefT, is_function<NoneRefT>::value>::type type;
		};
		/* forward */
		template <typename T>
		static inline T && forward(typename remove_reference<T>::type & val) noexcept {
			return static_cast<T &&>(val);
		}
		template <typename T>
		static inline T && forward(typename remove_reference<T>::type && val) noexcept {
			static_assert(!is_lvalue_reference<T>::value, "rvalue cannot be forwarded as an lvalue.");
			return static_cast<T &&>(val);
		}
	};

	/** Parser state. */
	enum ParserState {
		PST_START,         /**< Searching for start of token. */
		PST_GROUP,         /**< With group name. */
		PST_KEY,           /**< Within key name. */
		PST_ASSIGN,        /**< Expect assignment character next. */
		PST_VALUE,         /**< Expect value next. */
		PST_IGNORE_VALUE,  /**< Ignore value. */
		PST_STR_VALUE,     /**< Within string value. */
		PST_U32_VALUE,     /**< Within unsigned decimal number. */
		PST_HEX_U32_VALUE, /**< Within unsigned hexadecimal number. */
		PST_BLANK,         /**< Expect trailing blanks or comment. */
		PST_COMMENT,       /**< Within comment. */
		PST_I32_VALUE,     /**< Within signed decimal number. */
		PST_HEX_I32_VALUE, /**< Within signed hexadecimal number. */
		PST_ERROR          /**< Stopped parsing due to a syntax error. */
	};

	/* Forward declaration. */
	class Context;

	/**
	 * Helper class to make group/key comparison easier.
	 * All comparisons are done case-sensitive.
	 */
	class StringHelper {
		friend class Context;
	private:
		const char * str; /**< Null-terminated string. */

		/**
		 * Constructor.
		 *
		 * @param[in] s - pointer to a null-terminated string
		 * @remarks The storage of the given pointer needs to outlive this object.
		 * @remarks The string memory is not managed by this object.
		 */
		inline explicit StringHelper(const char * s) noexcept:
			str(s)
		{}
	public:
		/**
		 * Checks whether the string starts with the given prefix.
		 *
		 * @param[in] lit - prefix to test against
		 * @return true if prefixed by `lit`, else false
		 */
		inline bool startsWith(const char * lit) const noexcept {
			return strncmp(lit, this->str, strlen(lit)) == 0;
		}

		/**
		 * Cast operator.
		 *
		 * @return pointer to a null-terminated string
		 */
		inline operator const char *() const noexcept {
			return this->str;
		}

		/**
		 * Equal to comparison operator.
		 *
		 * @param[in] lit - string literal to compare with
		 * @return true if equal, else false
		 */
		inline bool operator== (const char * lit) const noexcept {
			return strcmp(this->str, lit) == 0;
		}

		/**
		 * Not equal to comparison operator.
		 *
		 * @param[in] lit - string literal to compare with
		 * @return false if equal, else true
		 */
		inline bool operator!= (const char * lit) const noexcept {
			return strcmp(this->str, lit) != 0;
		}

		/**
		 * Less-than comparison operator.
		 *
		 * @param[in] lit - string literal to compare with
		 * @return true if this string comes before, else false
		 */
		inline bool operator< (const char * lit) const noexcept {
			return strcmp(this->str, lit) < 0;
		}

		/**
		 * Greater-than comparison operator.
		 *
		 * @param[in] lit - string literal to compare with
		 * @return true if this string comes before, else false
		 */
		inline bool operator> (const char * lit) const noexcept {
			return strcmp(this->str, lit) > 0;
		}

		/**
		 * Less-than or equal to comparison operator.
		 *
		 * @param[in] lit - string literal to compare with
		 * @return true if this string comes before or is equal, else false
		 */
		inline bool operator<= (const char * lit) const noexcept {
			return strcmp(this->str, lit) <= 0;
		}

		/**
		 * Greater-than or equal to comparison operator.
		 *
		 * @param[in] lit - string literal to compare with
		 * @return true if this string comes before or is equal, else false
		 */
		inline bool operator>= (const char * lit) const noexcept {
			return strcmp(this->str, lit) >= 0;
		}
	};

	/** Parsing context which gets passed to the mapping provider. */
	class Context {
		friend struct IniParser;
	private:
		ParserState st; /**< Parsing state. */
		struct StrVal {
			char * ptr; /**< Value string receiving variable start pointer. */
			char * end; /**< Value string receiving variable end pointer (one past the null-terminator). */
		};
		union Num {
			uint32_t u32; /**< Unsigned number. */
			int32_t i32;  /**< Signed number. */
		};
		struct NumVal {
			Num * ptr; /**< Value number receiving variable pointer. */
			Num min;   /**< Minimum allowed number value (inclusive). */
			Num max;   /**< Maximum allowed number value (inclusive). */
		};
		union Val {
			StrVal str;
			NumVal num;
		};
		Val val;
		char * groupStr; /**< Current INI group. */
		char * keyStr;   /**< Current INI key. */
	public:
		StringHelper group; /**< Current INI group. */
		StringHelper key;   /**< Current INI key. */
	public:
		/**
		 * Constructor.
		 *
		 * @param[in,out] g - pointer to the buffer for the null-terminated group string
		 * @param[in,out] k - pointer to the buffer for the null-terminated key string
		 * @remark All buffers are managed by `IniParser`.
		 */
		explicit inline Context(char * g, char * k) noexcept:
			st(PST_START),
			groupStr(g),
			keyStr(k),
			group(groupStr),
			key(keyStr)
		{}

		/**
		 * Helper function to map a string variable.
		 *
		 * @param[out] var - character array variable to map
		 * @tparam N - number of characters in the given character array
		 */
		template <size_t N>
		inline void mapString(char (&var)[N]) noexcept {
			this->st = PST_STR_VALUE;
			this->val.str.ptr = var;
			this->val.str.end = var + N;
		}

		/**
		 * Helper function to map a string variable.
		 *
		 * @param[out] var - character array variable to map
		 * @param[int] size - maximum number of characters in the given character array including null-terminator
		 */
		inline void mapString(char * var, const size_t size) noexcept {
			this->st = PST_STR_VALUE;
			this->val.str.ptr = var;
			this->val.str.end = var + size;
		}

		/**
		 * Helper function to map a number variable.
		 *
		 * @param[out] var - number variable to map
		 * @param[in] valMin - minimum allowed value (inclusive)
		 * @param[in] valMax - maximum allowed value (inclusive)
		 */
		inline void mapNumber(uint32_t & var, const uint32_t valMin = 0, const uint32_t valMax = 0xFFFFFFFFUL) noexcept {
			this->st = PST_U32_VALUE;
			this->val.num.ptr = reinterpret_cast<Num *>(&var);
			this->val.num.min.u32 = valMin;
			this->val.num.max.u32 = valMax;
		}

		/**
		 * Helper function to map a number variable.
		 *
		 * @param[out] var - number variable to map
		 * @param[in] valMin - minimum allowed value (inclusive)
		 * @param[in] valMax - maximum allowed value (inclusive)
		 */
		inline void mapNumber(int32_t & var, const int32_t valMin = -0x80000000L, const int32_t valMax = 0x7FFFFFFFL) noexcept {
			this->st = PST_I32_VALUE;
			this->val.num.ptr = reinterpret_cast<Num *>(&var);
			this->val.num.min.i32 = valMin;
			this->val.num.max.i32 = valMax;
		}

		/**
		 * Helper function to map a number variable from hex value.
		 *
		 * @param[out] var - number variable to map
		 * @param[in] valMin - minimum allowed value (inclusive)
		 * @param[in] valMax - maximum allowed value (inclusive)
		 * @return new parsing state
		 */
		inline void mapHexNumber(uint32_t & var, const uint32_t valMin = 0, const uint32_t valMax = 0xFFFFFFFFUL) noexcept {
			this->st = PST_HEX_U32_VALUE;
			this->val.num.ptr = reinterpret_cast<Num *>(&var);
			this->val.num.min.u32 = valMin;
			this->val.num.max.u32 = valMax;
		}

		/**
		 * Helper function to map a number variable from hex value.
		 *
		 * @param[out] var - number variable to map
		 * @param[in] valMin - minimum allowed value (inclusive)
		 * @param[in] valMax - maximum allowed value (inclusive)
		 * @return new parsing state
		 */
		inline void mapHexNumber(int32_t & var, const int32_t valMin = -0x80000000L, const int32_t valMax = 0x7FFFFFFFL) noexcept {
			this->st = PST_HEX_I32_VALUE;
			this->val.num.ptr = reinterpret_cast<Num *>(&var);
			this->val.num.min.i32 = valMin;
			this->val.num.max.i32 = valMax;
		}
	};
private:
	/* Function object implementation for mapping provider. */
	struct MappingProviderIf {
		/** Calls the destructor of the function object. */
		void (* destroyer)(void *);
		/** Calls the function object and passes the parsing context. */
		bool (* invoker)(void * o, Context & ctx, const bool parsed);
		/**
		 * Returns the interface to handle the given function object type.
		 *
		 * @param[in] fn - function object
		 * @param[in] arg1 - parsing context
		 * @return interface to `Fn`
		 * @tparam Fn - function object type
		 * @tparam OrigFn - original function object type
		 * @tparam Arg1 - first function object argument type
		 */
		template <typename Fn, typename OrigFn, typename Arg1>
		static inline auto get(OrigFn fn, Arg1 arg1) noexcept
			-> const typename detail::enable_if<detail::is_same<decltype(fn(arg1)), bool>::value, MappingProviderIf>::type * {
			static const MappingProviderIf interface = {
				[] (void * o) { static_cast<Fn *>(o)->~Fn(); },
				[] (void * o, Context & ctx, const bool parsed) -> bool {
					if ( parsed ) {
						return true; /* no value verification */
					}
					return (*static_cast<Fn *>(o))(ctx);
				}
			};
			return &interface;
		}
		/**
		 * Returns the interface to handle the given function object type.
		 *
		 * @param[in] fn - function object
		 * @param[in] arg1 - parsing context
		 * @return interface to `Fn`
		 * @tparam Fn - function object type
		 * @tparam OrigFn - original function object type
		 * @tparam Arg1 - first function object argument type
		 */
		template <typename Fn, typename OrigFn, typename Arg1>
		static inline auto get(OrigFn fn, Arg1 arg1) noexcept
			-> const typename detail::enable_if<detail::is_same<decltype(fn(arg1, true)), bool>::value, MappingProviderIf>::type * {
			static const MappingProviderIf interface = {
				[] (void * o) {
					static_cast<Fn *>(o)->~Fn();
				},
				[] (void * o, Context & ctx, const bool parsed) -> bool {
					return (*static_cast<Fn *>(o))(ctx, parsed);
				}
			};
			return &interface;
		}
	};
	size_t maxIdLen; /**< Maximum number of characters for group/key strings including the null-terminator. */
	char * buffer; /**< Heap allocated buffer used to store the current group and key string as well as the mapping provider object. */
	Context ctx; /**< Paring context. */
	size_t line; /**< Current line. */
	size_t idx; /**< Current token string index (i.e. next character offset in output string). */
	size_t strBlank; /**< Index of the start of trailing blank characters in `ctx.val.str.ptr` or 0. */
	uint32_t num; /**< Current number value being parsed. */
	int lastCh; /**< Most recently parsed character. */
	bool numNeg; /**< True if the number being parsed has a negative sign, else false. */
	char quote; /**< Value starting quote character or 0 if none. */
	const MappingProviderIf * mappingProviderIf; /**< Interface to the mapping provider object depending on its type. */
	char inlineBuffer[1]; /**< Used when `buffer` is not explicitly allocated on heap but part of `IniParser`. */
public:
	/**
	 * Constructor.
	 *
	 * @param[in] mappingFn - user defined function which maps the values to variables
	 * @param[in] maxId - maximum number of characters for group and key strings including null-terminator
	 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
	 */
	template <typename MappingFn, typename DF = typename detail::decay_function<MappingFn>::type>
	inline explicit IniParser(MappingFn && mappingFn, const size_t maxId = 16):
		maxIdLen(maxId),
		buffer(new char[(maxIdLen * 2) + sizeof(DF)]),
		ctx(buffer, buffer + maxIdLen),
		line(1),
		lastCh(-1),
		quote(0),
		mappingProviderIf(MappingProviderIf::template get<DF>(mappingFn, ctx))
	{
		if (this->buffer != NULL) {
			const size_t groupKeySize = this->maxIdLen * 2;
			memset(this->buffer, 0, groupKeySize);
			new (this->buffer + groupKeySize) DF(detail::forward<MappingFn>(mappingFn));
		}
	}

	/**
	 * Copy constructor.
	 */
	IniParser(const IniParser &) = delete;

	/**
	 * Destructor.
	 */
	inline ~IniParser() noexcept {
		if (this->buffer != NULL) {
			const size_t groupKeySize = this->maxIdLen * 2;
			this->mappingProviderIf->destroyer(this->buffer + groupKeySize);
			if (this->buffer != this->inlineBuffer) {
				delete [] this->buffer;
			}
		}
	}

	/**
	 * Resets the parser to start new.
	 * Parsing -1 also resets most of the parser states.
	 */
	inline void reset() noexcept {
		this->ctx.st = PST_START;
		this->line = 1;
		this->lastCh = -1;
		this->quote = 0;
		memset(this->buffer, 0, this->maxIdLen * 2);
	}

	/**
	 * Returns the current line number starting at 1.
	 *
	 * @return current line
	 */
	inline size_t getLine() const noexcept {
		return this->line;
	}

	/**
	 * Converts to true if no parsing error has occurred, else false.
	 *
	 * @return parser in valid state?
	 */
	inline operator bool() const noexcept {
		return this->ctx.st != PST_ERROR;
	}

	/**
	 * Parses a single character of INI data.
	 *
	 * @param[in] ch - next INI data character or -1 on end of data
	 * @return true on success, else false on syntax error
	 */
	bool parse(const int ch) {
		bool isEndOfLineOrInput = false;
		size_t addLine = 0;
		ParserState newState;
		if (ch < 0) {
			isEndOfLineOrInput = true;
		} else if (ch == '\r') {
			isEndOfLineOrInput = true;
			addLine = 1;
		} else if (ch == '\n') {
			isEndOfLineOrInput = true;
			if (this->lastCh != '\r') {
				addLine = 1;
			}
		}
		this->lastCh = ch;
	onReevaluation:
		switch (this->ctx.st) {
		case PST_START:
			if (ch < 0) {
				/* end of file */
			} else if (ch == '[') {
				this->ctx.st = PST_GROUP;
				this->ctx.val.str.ptr = this->ctx.groupStr;
				this->ctx.val.str.end = this->ctx.groupStr + this->maxIdLen;
			} else if ( isalpha(ch) ) {
				this->ctx.st = PST_KEY;
				this->ctx.val.str.ptr = this->ctx.keyStr;
				this->ctx.val.str.end = this->ctx.keyStr + this->maxIdLen;
				*(this->ctx.val.str.ptr++) = char(ch);
			} else if (ch == '#') {
				this->ctx.st = PST_COMMENT;
			} else if ( ! isspace(ch) ) {
				goto onError; /* invalid character */
			}
			break;
		case PST_GROUP:
			if (this->ctx.val.str.ptr >= this->ctx.val.str.end) {
				goto onError; /* group string limit exceeded */
			} else if ( isValidIdChar(ch) ) {
				if (this->ctx.val.str.ptr == this->ctx.groupStr && ( ! isalpha(ch) )) {
					goto onError; /* invalid character */
				}
				*(this->ctx.val.str.ptr++) = char(ch);
			} else if (ch == ']') {
				this->ctx.st = PST_START;
				*(this->ctx.val.str.ptr) = 0;
			} else {
				goto onError; /* invalid character */
			}
			break;
		case PST_KEY:
			if (this->ctx.val.str.ptr >= this->ctx.val.str.end) {
				goto onError; /* key string limit exceeded */
			} else if ( isValidIdChar(ch) ) {
				*(this->ctx.val.str.ptr++) = char(ch);
			} else if ( isblank(ch) ) {
				this->ctx.st = PST_ASSIGN;
				*(this->ctx.val.str.ptr) = 0;
			} else if (ch == '=') {
				this->ctx.st = PST_VALUE;
			} else {
				goto onError; /* invalid character */
			}
			break;
		case PST_ASSIGN:
			if (ch == '=') {
				this->ctx.st = PST_VALUE;
			} else if ( ! isblank(ch) ) {
				goto onError; /* invalid character */
			}
			break;
		case PST_VALUE:
			if ( isblank(ch) ) {
				/* ignore */
			} else if (isValidStrChar(ch) || isEndOfLineOrInput) {
				this->ctx.st = PST_IGNORE_VALUE;
				const size_t groupKeySize = this->maxIdLen * 2;
				if ( ! this->mappingProviderIf->invoker(this->buffer + groupKeySize, this->ctx, false) ) {
					goto onError;
				}
				switch (this->ctx.st) {
				case PST_STR_VALUE:
					this->idx = 0;
					this->strBlank = 0;
					if (ch == '"' || ch == '\'') {
						this->quote = char(ch);
					} else {
						this->quote = 0;
						goto onReevaluation;
					}
					break;
				case PST_U32_VALUE:
				case PST_HEX_U32_VALUE:
					this->idx = 0;
					this->num = 0;
					this->numNeg = false;
					goto onReevaluation;
				case PST_I32_VALUE:
				case PST_HEX_I32_VALUE:
					this->idx = 0;
					this->num = 0;
					this->numNeg = (ch == '-');
					if ( ! this->numNeg ) {
						/* readjust range and treat as unsigned */
						if (this->ctx.val.num.min.i32 < 0) {
							this->ctx.val.num.min.u32 = 0;
						}
						if (this->ctx.val.num.max.i32 < 0) {
							goto onError; /* number out of range */
						}
						if (this->ctx.st == PST_I32_VALUE) {
							this->ctx.st = PST_U32_VALUE;
						} else {
							this->ctx.st = PST_HEX_U32_VALUE;
						}
						goto onReevaluation;
					}
					/* convert to signed after parsing as unsigned */
					if (this->ctx.st == PST_I32_VALUE) {
						this->ctx.st = PST_U32_VALUE;
					} else {
						this->ctx.st = PST_HEX_U32_VALUE;
					}
					break;
				case PST_IGNORE_VALUE:
					if (ch == '"' || ch == '\'') {
						this->quote = char(ch);
					} else {
						this->quote = 0;
					}
					break;
				default:
					goto onError; /* invalid state from mapping provider */
				}
				if ( isEndOfLineOrInput ) {
					/* empty value */
					goto onReevaluation;
				}
			} else {
				goto onError; /* invalid character */
			}
			break;
		case PST_IGNORE_VALUE:
			if (ch == '#' && this->quote == 0) {
				this->ctx.st = PST_COMMENT;
			} else if (ch == this->quote && this->quote != 0) {
				this->ctx.st = PST_BLANK;
			} else if (isEndOfLineOrInput && this->quote == 0) {
				this->ctx.st = PST_START;
			} else if ( isValidStrChar(ch) ) {
				/* ignore */
			} else {
				goto onError; /* invalid character */
			}
			break;
		case PST_STR_VALUE:
			newState = this->ctx.st;
			if ((this->ctx.val.str.ptr + this->idx) >= this->ctx.val.str.end) {
				goto onError; /* value string limit exceeded */
			} else if (ch == '#' && this->quote == 0) {
				newState = PST_COMMENT;
			} else if (ch == this->quote && this->quote != 0) {
				newState = PST_BLANK;
			} else if (isEndOfLineOrInput && this->quote == 0) {
				newState = PST_START;
			} else if ( isValidStrChar(ch) ) {
				if ( isblank(ch) ) {
					if (this->strBlank == 0) {
						this->strBlank = this->idx;
					}
				} else {
					this->strBlank = 0;
				}
				this->ctx.val.str.ptr[this->idx++] = char(ch);
			} else {
				goto onError; /* invalid character */
			}
			if (newState != this->ctx.st) {
				this->trimString();
				const size_t groupKeySize = this->maxIdLen * 2;
				if ( ! this->mappingProviderIf->invoker(this->buffer + groupKeySize, this->ctx, true) ) {
					goto onError;
				}
				this->ctx.st = newState;
			}
			break;
		case PST_U32_VALUE:
			if ( isdigit(ch) ) {
				this->idx++;
				const uint32_t oldNum = this->num;
				this->num = (this->num * 10) + uint32_t(ch - '0');
				if (oldNum > this->num) {
					goto onError; /* number overflow */
				}
			} else if ( isEndOfLineOrInput ) {
				this->ctx.st = PST_START;
			} else if (this->idx == 1 && this->num == 0 && ch == 'x') {
				this->ctx.st = PST_HEX_U32_VALUE;
			} else if (ch == '#') {
				this->ctx.st = PST_COMMENT;
			} else if ( isspace(ch) ) {
				this->ctx.st = PST_BLANK;
			} else {
				goto onError; /* invalid character */
			}
			if (this->ctx.st != PST_U32_VALUE) {
				if (this->idx == 0) {
					goto onError; /* missing value */
				} else {
					goto onCheckNumRange;
				}
			}
			break;
		case PST_HEX_U32_VALUE:
			if ( isxdigit(ch) ) {
				this->idx++;
				const uint32_t oldNum = this->num;
				if (ch <= '9') {
					this->num = (this->num << 4) + uint32_t(ch - '0');
				} else if (ch <= 'F') {
					this->num = (this->num << 4) + uint32_t(ch - 'A' + 10);
				} else {
					this->num = (this->num << 4) + uint32_t(ch - 'a' + 10);
				}
				if (oldNum > this->num) {
					goto onError; /* number overflow */
				}
			} else if ( isEndOfLineOrInput ) {
				this->ctx.st = PST_START;
			} else if (ch == '#') {
				this->ctx.st = PST_COMMENT;
			} else if ( isspace(ch) ) {
				this->ctx.st = PST_BLANK;
			} else {
				goto onError; /* invalid character */
			}
			if (this->ctx.st != PST_HEX_U32_VALUE) {
				if (this->idx == 0) {
					goto onError; /* missing value */
				} else {
					goto onCheckNumRange;
				}
			}
			break;
		case PST_BLANK:
			if (ch == '#') {
				this->ctx.st = PST_COMMENT;
			} else if ( isEndOfLineOrInput ) {
				this->ctx.st = PST_START;
			} else if ( ! isblank(ch) ) {
				goto onError; /* invalid character */
			}
			break;
		case PST_COMMENT:
			if ( isEndOfLineOrInput ) {
				this->ctx.st = PST_START;
			}
			break;
		default: /* PST_I32_VALUE | PST_HEX_I32_VALUE | PST_ERROR */
			goto onError;
		}
		this->line += addLine;
		return true;
	onCheckNumRange:
		if ( this->numNeg ) {
			/* parsed unsigned number for signed target -> convert and check range accordingly */
			if (this->num > 0x80000000UL) {
				goto onError; /* number overflow */
			}
			const int32_t numI32 = -int32_t(this->num);
			if (numI32 >= this->ctx.val.num.min.i32 && numI32 <= this->ctx.val.num.max.i32) {
				this->ctx.val.num.ptr->i32 = numI32;
				const size_t groupKeySize = this->maxIdLen * 2;
				if ( ! this->mappingProviderIf->invoker(this->buffer + groupKeySize, this->ctx, true) ) {
					goto onError;
				}
				this->line += addLine;
				return true; /* number within range */
			}
		} else if (this->num >= this->ctx.val.num.min.u32 && this->num <= this->ctx.val.num.max.u32) {
			this->ctx.val.num.ptr->u32 = this->num; /* same for signed number in our case */
			const size_t groupKeySize = this->maxIdLen * 2;
			if ( ! this->mappingProviderIf->invoker(this->buffer + groupKeySize, this->ctx, true) ) {
				goto onError;
			}
			this->line += addLine;
			return true; /* number within range */
		}
		/* fall-through */
	onError:
		this->ctx.st = PST_ERROR;
		return false;
	}

	/**
	 * Convenience function to parse an INI from string.
	 *
	 * @param[in] str - null-terminated INI string to parse
	 * @param[in] mappingFn - user defined function which maps the values to variables
	 * @param[in] maxId - maximum number of characters for group and key strings including null-terminator
	 * @return line number with a syntax error or 0 on success
	 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
	 */
	template <typename MappingFn>
	static inline size_t parseString(const char * str, MappingFn && mappingFn, const size_t maxId = 16) {
		IniParser ini(mappingFn, maxId);
		char ch;
		do {
			ch = *str++;
			if ( ! ini.parse((ch == 0) ? -1 : int(ch)) ) {
				return ini.getLine();
			}
		} while (ch != 0);
		return 0;
	}

	/**
	 * Convenience function to parse an INI from string.
	 *
	 * @param[in] str - INI string to parse
	 * @param[in] len - maximum number of bytes in the INI string
	 * @param[in] mappingFn - user defined function which maps the values to variables
	 * @param[in] maxId - maximum number of characters for group and key strings including null-terminator
	 * @return line number with a syntax error or 0 on success
	 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
	 */
	template <typename MappingFn>
	static inline size_t parseString(const char * str, const size_t len, MappingFn && mappingFn, const size_t maxId = 16) {
		IniParser ini(mappingFn, maxId);
		for (size_t i = 0; i <= len; i++) {
			if (*str == 0) {
				if ( ! ini.parse(-1) ) {
					return ini.getLine();
				}
				break;
			} else if ( ! ini.parse((i == len) ? -1 : int(*str++)) ) {
				return ini.getLine();
			}
		}
		return 0;
	}

	/**
	 * Convenience function to parse an INI from a data provider function.
	 *
	 * @param[in] dataFn - user defined function with feeds the parser with data
	 * @param[in] mappingFn - user defined function which maps the values to variables
	 * @param[in] maxId - maximum number of characters for group and key strings including null-terminator
	 * @return line number with a syntax error or 0 on success
	 * @tparam DataFn - function object with the signature `int function(void)` returning -1 at the end
	 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
	 */
	template <typename DataFn, typename MappingFn>
	static inline size_t parseFn(DataFn && dataFn, MappingFn && mappingFn, const size_t maxId = 16) {
		IniParser ini(mappingFn, maxId);
		int ch;
		do {
			ch = dataFn();
			if ( ! ini.parse(ch) ) {
				return ini.getLine();
			}
		} while (ch >= 0);
		return 0;
	}
private:
	/**
	 * Constructor.
	 *
	 * @param[in] mappingFn - user defined function which maps the values to variables
	 * @tparam Params - buffer parameters
	 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
	 */
	template <typename Params, typename MappingFn, typename DF = typename detail::decay_function<MappingFn>::type>
	inline explicit IniParser(Params, MappingFn && mappingFn):
		maxIdLen(Params::maxId),
		buffer(inlineBuffer),
		ctx(buffer, buffer + maxIdLen),
		line(1),
		lastCh(-1),
		quote(0),
		mappingProviderIf(MappingProviderIf::template get<DF>(mappingFn, ctx))
	{
		static_assert(Params::n >= sizeof(DF), "Provided mapping function is too large for the internal buffer.");
		const size_t groupKeySize = this->maxIdLen * 2;
		memset(this->buffer, 0, groupKeySize);
		new (this->buffer + groupKeySize) DF(detail::forward<MappingFn>(mappingFn));
	}

	/**
	 * Trims the completely parsed value string according to
	 * leading spaces and quotation.
	 */
	inline void trimString() noexcept {
		if (this->quote == 0 && this->strBlank != 0) {
			this->ctx.val.str.ptr[this->strBlank] = 0;
		} else {
			this->ctx.val.str.ptr[this->idx] = 0;
		}
	};

	/**
	 * Tests whether the given character is a valid group or key character.
	 * This does not apply to the first character, which needs to be a letter.
	 *
	 * @param[in] ch - character to test
	 * @return true if valid ID character, else false
	 */
	static bool isValidIdChar(const int ch) noexcept {
		return isalnum(ch) || ch == '_' || ch == '.';
	}

	/**
	 * Tests whether the given character is a valid string value character.
	 *
	 * @param[in] ch - character to test
	 * @return true if valid string value character, else false
	 */
	static bool isValidStrChar(const int ch) noexcept {
		return ch != 127 && (ch == '\t' || ch >= ' ');
	}
};


/**
 * Streaming INI parser.
 * Same as `IniParser` but without any heap allocation.
 *
 * @tparam MaxId - maximum number of characters for group and key strings including null-terminator
 * @tparam N - number of bytes to allocate for the mapping provider function object
 */
template <size_t MaxId, size_t N>
class IniParserSized : public IniParser {
public:
	/**
	 * Used to hand over template parameters to the base constructor.
	 */
	struct Params {
		/** maximum number of characters for group and key strings including null-terminator */
		static constexpr size_t maxId = MaxId;
		/** number of bytes to allocate for the mapping provider function object */
		static constexpr size_t n = N;
	};
private:
	char extendedBuffer[size_t((2 * MaxId) + N - 1)];
public:
	/**
	 * Constructor.
	 *
	 * @param[in] mappingFn - user defined function which maps the values to variables
	 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
	 */
	template <typename MappingFn>
	inline explicit IniParserSized(MappingFn && mappingFn):
		IniParser(Params(), mappingFn)
	{}
};


/**
 * Convenience function to parse an INI from string.
 *
 * @param[in] str - null-terminated INI string to parse
 * @param[in] mappingFn - user defined function which maps the values to variables
 * @return line number with a syntax error or 0 on success
 * @tparam MaxId - maximum number of characters for group and key strings including null-terminator
 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
 */
template <size_t MaxId, typename MappingFn, typename DF = typename IniParser::detail::decay_function<MappingFn>::type>
static inline size_t iniParseString(const char * str, MappingFn && mappingFn) {
	IniParserSized<MaxId, sizeof(DF)> ini(mappingFn);
	char ch;
	do {
		ch = *str++;
		if ( ! ini.parse((ch == 0) ? -1 : int(ch)) ) {
			return ini.getLine();
		}
	} while (ch != 0);
	return 0;
}


/**
 * Convenience function to parse an INI from string.
 *
 * @param[in] str - INI string to parse
 * @param[in] len - maximum number of bytes in the INI string
 * @param[in] mappingFn - user defined function which maps the values to variables
 * @return line number with a syntax error or 0 on success
 * @tparam MaxId - maximum number of characters for group and key strings including null-terminator
 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
 */
template <size_t MaxId, typename MappingFn, typename DF = typename IniParser::detail::decay_function<MappingFn>::type>
static inline size_t iniParseString(const char * str, const size_t len, MappingFn && mappingFn) {
	IniParserSized<MaxId, sizeof(DF)> ini(mappingFn);
	for (size_t i = 0; i <= len; i++) {
		if (*str == 0) {
			if ( ! ini.parse(-1) ) {
				return ini.getLine();
			}
			break;
		} else if ( ! ini.parse((i == len) ? -1 : int(*str++)) ) {
			return ini.getLine();
		}
	}
	return 0;
}


/**
 * Convenience function to parse an INI from a data provider function.
 *
 * @param[in] dataFn - user defined function with feeds the parser with data
 * @param[in] mappingFn - user defined function which maps the values to variables
 * @return line number with a syntax error or 0 on success
 * @tparam MaxId - maximum number of characters for group and key strings including null-terminator
 * @tparam DataFn - function object with the signature `int function(void)` returning -1 at the end
 * @tparam MappingFn - function object with the signature `bool function(IniParser::Context &)` returning true on success, and false otherwise
 */
template <size_t MaxId, typename DataFn, typename MappingFn, typename DF = typename IniParser::detail::decay_function<MappingFn>::type>
static inline size_t iniParseFn(DataFn && dataFn, MappingFn && mappingFn) {
	IniParserSized<MaxId, sizeof(DF)> ini(mappingFn);
	int ch;
	do {
		ch = dataFn();
		if ( ! ini.parse(ch) ) {
			return ini.getLine();
		}
	} while (ch >= 0);
	return 0;
}


#endif /* _INI_PARSER_HPP_ */
