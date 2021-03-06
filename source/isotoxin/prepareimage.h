#pragma once

class dialog_prepareimage_c;
template<> struct MAKE_ROOT<dialog_prepareimage_c> : public _PROOT(dialog_prepareimage_c)
{
    contact_key_s ck;
    ts::bitmap_c bitmap;
    MAKE_ROOT(bool, const contact_key_s &ck) : _PROOT(dialog_prepareimage_c)(), ck(ck)  { init(RS_NORMAL_MAINPARENT); }
    MAKE_ROOT(bool, const contact_key_s &ck, const ts::bitmap_c &bitmap) : _PROOT(dialog_prepareimage_c)(), ck(ck), bitmap(bitmap)  { init(RS_NORMAL_MAINPARENT); }
    ~MAKE_ROOT() {}
};

class dialog_prepareimage_c : public gui_isodialog_c
{
    typedef gui_isodialog_c super;
    GM_RECEIVER(dialog_prepareimage_c, GM_DROPFILES);

    struct saver_s : public ts::task_c
    {
        dialog_prepareimage_c *dlg;
        ts::bitmap_c    bitmap2save;
        ts::blob_c      saved_jpg;
        ts::img_format_e saved_img_format = ts::if_none;

        saver_s(dialog_prepareimage_c *dlg, ts::bitmap_c bitmap2save) :dlg(dlg), bitmap2save(bitmap2save)
        {
        }

        /*virtual*/ int iterate(ts::task_executor_c *e) override;
        /*virtual*/ void done(bool canceled) override;
    } *saver = nullptr;

    contact_key_s ck;
    ts::blob_c  loaded_image;
    ts::blob_c  saved_image;
    ts::img_format_e loaded_img_format = ts::if_none;
    ts::img_format_e saved_img_format = ts::if_none;

    void saver_job_done(saver_s *s);

    process_animation_s pa;
    UNIQUE_PTR(vsb_c) camera;
    ts::Time show_cam_res_deadline = ts::Time::undefined();
    enum
    {
        SCR_NO,
        SCR_YES_WAIT,
        SCR_YES_COUNTDOWN,
    } show_cam_resolution = SCR_NO;

    framedrawer_s fd;
    ts::animated_c anm;

    ts::bitmap_c bitmap;    // original
    ts::bitmap_c image;     // premultiplied fit to rect

    ts::irect out;
    ts::irect imgrect;
    ts::irect viewrect;
    ts::ivec2 offset;
    ts::irect bitmapcroprect;
    ts::irect inforect;
    ts::irect croprect;
    ts::irect storecroprect;
    float bckscale = 1.0f;

    void backscale_croprect();

    ts::ivec2 wsz;

    ts::ivec2 user_offset = ts::ivec2(0);
    ts::uint32 area = 0;
    int tickvalue = 0;
    int prevsize = 0;
    ts::Time next_frame_tick = ts::Time::past();

    ts::shared_ptr<theme_rect_s> shadow;

    enum camst_e
    {
        CAMST_NONE,
        CAMST_VIEW,
        CAMST_WAIT_ORIG_SIZE,
    } camst = CAMST_NONE;

    bool alpha = false;
    bool dirty = true;
    bool disabled_ok = false;
    bool animated = false;
    bool videodevicesloaded = false;
    bool selrectvisible = false;
    bool allowdndinfo = true;

    void nextframe();

    void update_info();

    void on_newimage();
    bool prepare_working_image(RID r = RID(), GUIPARAM p = nullptr);

    bool animation(RID, GUIPARAM);
    void prepare_stuff();
    void clamp_user_offset();
    void resave();

    bool space_key(RID, GUIPARAM);
    bool paste_hotkey_handler(RID, GUIPARAM);

    vsb_list_t video_devices;
    bool start_capture_menu(RID, GUIPARAM);
    void start_capture_menu_sel(const ts::str_c& prm);
    void start_capture_menu_sel_res( const ts::str_c& prm );
    void start_capture(const vsb_descriptor_s &desc, const ts::wstr_c &res );

    void draw_process(ts::TSCOLOR col, bool cam, bool cambusy);

    void allow_ok( bool allow );

protected:
    /*virtual*/ int unique_tag() override { return UD_PREPARE_IMAGE; }
    /*virtual*/ void created() override;

    bool open_image(RID, GUIPARAM);
    void load_image(const ts::wstr_c &fn);

public:
    dialog_prepareimage_c(MAKE_ROOT<dialog_prepareimage_c> &data);
    ~dialog_prepareimage_c();

    /*virtual*/ ts::uint32 caption_buttons() const override { return SETBIT(CBT_MAXIMIZE) | SETBIT(CBT_NORMALIZE) | super::caption_buttons(); }
    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;

    /*virtual*/ void children_repos() override
    {
        super::children_repos();
        dirty = true;
    }

    void set_video_devices(vsb_list_t &&_video_devices);

};


class desktopgrab_c;
template<> struct MAKE_ROOT<desktopgrab_c> : public _PROOT( desktopgrab_c )
{
    ts::irect r;
    uint64 avk;
    int monitor;
    bool av_call;
    MAKE_ROOT( const ts::irect &r, uint64 avk, int monitor, bool av_call ) :_PROOT( desktopgrab_c )( ), r( r ), avk( avk ), monitor(monitor), av_call( av_call ) { init( RS_TOOL ); }
    ~MAKE_ROOT();
};

class desktopgrab_c : public gui_control_c
{
    typedef gui_control_c super;

    GM_RECEIVER( desktopgrab_c, ISOGM_GRABDESKTOPEVENT );
    bool esc_handler( RID, GUIPARAM );

    ts::irect orr = ts::irect(0);
    ts::irect hole = ts::irect( 0 );

    int monitor = 0;
    int tickvalue = 0;
    framedrawer_s fd;
    uint64 avk;

    bool av_call;

    bool ticktick( RID, GUIPARAM );
protected:
    /*virtual*/ void created() override;

public:
    desktopgrab_c( MAKE_ROOT<desktopgrab_c> &data );
    ~desktopgrab_c();

    /*virtual*/ ts::wstr_c get_name() const override { return ts::wstr_c(); }
    /*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override;

    static void run( uint64 avkey, bool av_call );

};
