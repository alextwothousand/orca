/* This file is generated from specs/discord/user.endpoints-params.json, Please don't edit it. */
/**
 * @file specs-code/discord/user.endpoints-params.h
 * @see https://discord.com/developers/docs/resources/user
 */


// Modify Current User
// defined at specs/discord/user.endpoints-params.json:9:22
/**
 * @verbatim embed:rst:leading-asterisk
 * .. container:: toggle

 *   .. container:: header

 *     **Methods**

 *   * Initializer:

 *     * :code:`void discord_modify_current_user_params_init(struct discord_modify_current_user_params *)`
 *   * Cleanup:

 *     * :code:`void discord_modify_current_user_params_cleanup(struct discord_modify_current_user_params *)`
 *     * :code:`void discord_modify_current_user_params_list_free(struct discord_modify_current_user_params **)`
 *   * JSON Decoder:

 *     * :code:`void discord_modify_current_user_params_from_json(char *rbuf, size_t len, struct discord_modify_current_user_params **)`
 *     * :code:`void discord_modify_current_user_params_list_from_json(char *rbuf, size_t len, struct discord_modify_current_user_params ***)`
 *   * JSON Encoder:

 *     * :code:`void discord_modify_current_user_params_to_json(char *wbuf, size_t len, struct discord_modify_current_user_params *)`
 *     * :code:`void discord_modify_current_user_params_list_to_json(char *wbuf, size_t len, struct discord_modify_current_user_params **)`
 * @endverbatim
 */
struct discord_modify_current_user_params {
  /* specs/discord/user.endpoints-params.json:12:20
     '{ "name": "username", "type":{ "base":"char", "dec":"*" }}' */
  char *username;

  /* specs/discord/user.endpoints-params.json:13:20
     '{ "name": "avatar", "type":{ "base":"char", "dec":"*"}, "comment":"base64 encoded image data"}' */
  char *avatar; ///< base64 encoded image data

  // The following is metadata used to 
  // 1. control which field should be extracted/injected
  // 2. record which field is presented(defined) in JSON
  // 3. record which field is null in JSON
/// @cond DOXYGEN_SHOULD_SKIP_THIS
  struct {
    bool enable_arg_switches;
    bool enable_record_defined;
    bool enable_record_null;
    void *arg_switches[2];
    void *record_defined[2];
    void *record_null[2];
  } __M; // metadata
/// @endcond
};
extern void discord_modify_current_user_params_cleanup_v(void *p);
extern void discord_modify_current_user_params_cleanup(struct discord_modify_current_user_params *p);
extern void discord_modify_current_user_params_init_v(void *p);
extern void discord_modify_current_user_params_init(struct discord_modify_current_user_params *p);
extern void discord_modify_current_user_params_from_json_v(char *json, size_t len, void *pp);
extern void discord_modify_current_user_params_from_json(char *json, size_t len, struct discord_modify_current_user_params **pp);
extern size_t discord_modify_current_user_params_to_json_v(char *json, size_t len, void *p);
extern size_t discord_modify_current_user_params_to_json(char *json, size_t len, struct discord_modify_current_user_params *p);
extern void discord_modify_current_user_params_list_free_v(void **p);
extern void discord_modify_current_user_params_list_free(struct discord_modify_current_user_params **p);
extern void discord_modify_current_user_params_list_from_json_v(char *str, size_t len, void *p);
extern void discord_modify_current_user_params_list_from_json(char *str, size_t len, struct discord_modify_current_user_params ***p);
extern size_t discord_modify_current_user_params_list_to_json_v(char *str, size_t len, void *p);
extern size_t discord_modify_current_user_params_list_to_json(char *str, size_t len, struct discord_modify_current_user_params **p);

// Create Group DM
// defined at specs/discord/user.endpoints-params.json:19:22
/**
 * @verbatim embed:rst:leading-asterisk
 * .. container:: toggle

 *   .. container:: header

 *     **Methods**

 *   * Initializer:

 *     * :code:`void discord_create_group_dm_params_init(struct discord_create_group_dm_params *)`
 *   * Cleanup:

 *     * :code:`void discord_create_group_dm_params_cleanup(struct discord_create_group_dm_params *)`
 *     * :code:`void discord_create_group_dm_params_list_free(struct discord_create_group_dm_params **)`
 *   * JSON Decoder:

 *     * :code:`void discord_create_group_dm_params_from_json(char *rbuf, size_t len, struct discord_create_group_dm_params **)`
 *     * :code:`void discord_create_group_dm_params_list_from_json(char *rbuf, size_t len, struct discord_create_group_dm_params ***)`
 *   * JSON Encoder:

 *     * :code:`void discord_create_group_dm_params_to_json(char *wbuf, size_t len, struct discord_create_group_dm_params *)`
 *     * :code:`void discord_create_group_dm_params_list_to_json(char *wbuf, size_t len, struct discord_create_group_dm_params **)`
 * @endverbatim
 */
struct discord_create_group_dm_params {
  /* specs/discord/user.endpoints-params.json:22:20
     '{ "name": "access_tokens", "type":{ "base":"ja_str", "dec":"ntl" }, 
          "comment":"access tokens of users that have granted your app the gdm.join scope"}' */
  ja_str **access_tokens; ///< access tokens of users that have granted your app the gdm.join scope

  /* specs/discord/user.endpoints-params.json:24:19
     '{ "name":"nicks", "type":{ "base":"ja_u64", "dec":"ntl"}, 
          "comment":"a dictionary of user ids to their respective nicknames"}' */
  ja_u64 **nicks; ///< a dictionary of user ids to their respective nicknames

  // The following is metadata used to 
  // 1. control which field should be extracted/injected
  // 2. record which field is presented(defined) in JSON
  // 3. record which field is null in JSON
/// @cond DOXYGEN_SHOULD_SKIP_THIS
  struct {
    bool enable_arg_switches;
    bool enable_record_defined;
    bool enable_record_null;
    void *arg_switches[2];
    void *record_defined[2];
    void *record_null[2];
  } __M; // metadata
/// @endcond
};
extern void discord_create_group_dm_params_cleanup_v(void *p);
extern void discord_create_group_dm_params_cleanup(struct discord_create_group_dm_params *p);
extern void discord_create_group_dm_params_init_v(void *p);
extern void discord_create_group_dm_params_init(struct discord_create_group_dm_params *p);
extern void discord_create_group_dm_params_from_json_v(char *json, size_t len, void *pp);
extern void discord_create_group_dm_params_from_json(char *json, size_t len, struct discord_create_group_dm_params **pp);
extern size_t discord_create_group_dm_params_to_json_v(char *json, size_t len, void *p);
extern size_t discord_create_group_dm_params_to_json(char *json, size_t len, struct discord_create_group_dm_params *p);
extern void discord_create_group_dm_params_list_free_v(void **p);
extern void discord_create_group_dm_params_list_free(struct discord_create_group_dm_params **p);
extern void discord_create_group_dm_params_list_from_json_v(char *str, size_t len, void *p);
extern void discord_create_group_dm_params_list_from_json(char *str, size_t len, struct discord_create_group_dm_params ***p);
extern size_t discord_create_group_dm_params_list_to_json_v(char *str, size_t len, void *p);
extern size_t discord_create_group_dm_params_list_to_json(char *str, size_t len, struct discord_create_group_dm_params **p);
