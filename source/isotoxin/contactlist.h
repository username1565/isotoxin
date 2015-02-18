#pragma once

enum contact_item_role_e
{
    CIR_LISTITEM,
    CIR_ME,
    CIR_CONVERSATION_HEAD,
    CIR_METACREATE,
    CIR_DNDOBJ,
};

class gui_contact_item_c;
template<> struct MAKE_CHILD<gui_contact_item_c> : public _PCHILD(gui_contact_item_c)
{
    contact_item_role_e role = CIR_LISTITEM;
    contact_c *contact;
    MAKE_CHILD(RID parent_, contact_c *c):contact(c) { parent = parent_; }
    ~MAKE_CHILD();
    MAKE_CHILD &operator << (contact_item_role_e r) { role = r; return *this; }
};
template<> struct MAKE_ROOT<gui_contact_item_c> : public _PROOT(gui_contact_item_c)
{
    contact_c *contact;
    MAKE_ROOT(drawchecker &dch, contact_c *c) : _PROOT(gui_contact_item_c)(dch), contact(c) { init(false); }
    ~MAKE_ROOT() {}
};

class gui_contact_item_c : public gui_label_c
{
    DUMMY(gui_contact_item_c);
    ts::shared_ptr<contact_c> contact;
    contact_item_role_e role = CIR_LISTITEM;
    GM_RECEIVER( gui_contact_item_c, ISOGM_SELECT_CONTACT );
    GM_RECEIVER( gui_contact_item_c, ISOGM_SOMEUNREAD );
    
    

    static const ts::flags32_s::BITS  F_PROTOHIT    = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS  F_EDITNAME    = FLAGS_FREEBITSTART_LABEL << 1;
    static const ts::flags32_s::BITS  F_EDITSTATUS  = FLAGS_FREEBITSTART_LABEL << 2;
    static const ts::flags32_s::BITS  F_SKIPUPDATE  = FLAGS_FREEBITSTART_LABEL << 3;
    static const ts::flags32_s::BITS  F_LBDN        = FLAGS_FREEBITSTART_LABEL << 4;
    static const ts::flags32_s::BITS  F_DNDDRAW     = FLAGS_FREEBITSTART_LABEL << 5;

    friend class contact_c;
    friend class contacts_c;
    int n_unread =0;

    bool audio_call(RID, GUIPARAM);
    bool send_file(RID, GUIPARAM);

    bool edit0(RID, GUIPARAM);
    bool edit1(RID, GUIPARAM);
    void updrect(void *, int r, const ts::ivec2 &);

public:
    gui_contact_item_c(MAKE_ROOT<gui_contact_item_c> &data);
    gui_contact_item_c(MAKE_CHILD<gui_contact_item_c> &data);
    /*virtual*/ ~gui_contact_item_c();

    /*virtual*/ void update_dndobj(guirect_c *donor) override;
    /*virtual*/ guirect_c * summon_dndobj(const ts::ivec2 &deltapos) override;

    void target(bool tgt); // d'n'd target
    void on_drop(gui_contact_item_c *ondr);

    bool is_protohit() const {return flags.is(F_PROTOHIT);}
    void protohit() { flags.set(F_PROTOHIT); }
    bool update_buttons( RID r = RID(), GUIPARAM p = nullptr );
    bool cancel_edit( RID r = RID(), GUIPARAM p = nullptr);
    bool apply_edit( RID r = RID(), GUIPARAM p = nullptr);

    contact_item_role_e getrole() const {return role;}

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void resetcontact()
    {
        if (flags.is(F_SKIPUPDATE)) return;
        contact = nullptr; update_text();
    }
    void setcontact(contact_c *c);
    contact_c &getcontact() {return SAFE_REF(contact.get());}
    const contact_c &getcontact() const {return SAFE_REF(contact.get());}
    bool contacted() const {return contact != nullptr;}
    void update_text();

    bool is_after(gui_contact_item_c &ci); // sort comparsion
};

enum contact_list_role_e
{
    CLR_MAIN_LIST,
    CLR_NEW_METACONTACT_LIST,
};

class gui_contactlist_c;
template<> struct MAKE_CHILD<gui_contactlist_c> : public _PCHILD(gui_contactlist_c)
{
    contact_list_role_e role = CLR_MAIN_LIST;
    MAKE_CHILD(RID parent_, contact_list_role_e role = CLR_MAIN_LIST) :role(role) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_contactlist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_contactlist_c);

    GM_RECEIVER(gui_contactlist_c, ISOGM_CHANGED_PROFILEPARAM);
    GM_RECEIVER(gui_contactlist_c, ISOGM_UPDATE_CONTACT_V);
    GM_RECEIVER(gui_contactlist_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_contactlist_c, GM_DRAGNDROP);

    uint64 sort_tag = 0;
    contact_list_role_e role = CLR_MAIN_LIST;

    int skip_top_pixels = 100;
    int skip_bottom_pixels = 70;
    int skipctl = 0;

    ts::safe_ptr<gui_contact_item_c> self;
    ts::safe_ptr<gui_contact_item_c> dndtarget;

    ts::array_inplace_t<contact_key_s, 2> * arr = nullptr;

    //void check_focus(RID r, GUIPARAM p);
    //void update_size(RID r, GUIPARAM p);
    //void die() { TSDEL(this); }

    /*virtual*/ void children_repos_info(cri_s &info) const override;

public:
    gui_contactlist_c( MAKE_CHILD<gui_contactlist_c> &data) :gui_vscrollgroup_c(data) { defaultthrdraw = DTHRO_BASE; role = data.role; }
    /*virtual*/ ~gui_contactlist_c();

    void array_mode( ts::array_inplace_t<contact_key_s, 2> & arr );
    void refresh_array();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_KEEP;}
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

};