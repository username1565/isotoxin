#include "rectangles.h"

void fixrect(ts::irect &r, const ts::ivec2 &minsz, const ts::ivec2 &maxsz, ts::uint32 hitarea)
{
    if (r.width() < minsz.x)
    {
        if (0 != (hitarea & AREA_LEFT))
        {
            r.lt.x = r.rb.x - minsz.x;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_RITE))
        {
            r.rb.x = r.lt.x + minsz.x;
        }
        else ERROR("fixrect failure");
    }
    if (r.width() > maxsz.x)
    {
        if (0 != (hitarea & AREA_LEFT))
        {
            r.lt.x = r.rb.x - maxsz.x;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_RITE))
        {
            r.rb.x = r.lt.x + maxsz.x;
        }
        else ERROR("fixrect failure");
    }

    if (r.height() < minsz.y)
    {
        if (0 != (hitarea & AREA_TOP))
        {
            r.lt.y = r.rb.y - minsz.y;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_BOTTOM))
        {
            r.rb.y = r.lt.y + minsz.y;
        }
        else ERROR("fixrect failure");
    }
    if (r.height() > maxsz.y)
    {
        if (0 != (hitarea & AREA_TOP))
        {
            r.lt.y = r.rb.y - maxsz.y;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_BOTTOM))
        {
            r.rb.y = r.lt.y + maxsz.y;
        }
        else ERROR("fixrect failure");
    }

}

static const int minsbsize = 20;

void calcsb( int &pos, int &size, int shift, int viewsize, int datasize )
{
    if (viewsize >= datasize)
    {
        pos = 0;
        size = viewsize;
        return;
    }

    float k = float(viewsize) / float(datasize);
    float sbmove = (float)(viewsize - minsbsize);
    float datamove = float(datasize - viewsize);
    size = lround(ts::tmax<float>( k * viewsize, minsbsize ));
    sbmove = float(viewsize - size);


    pos = lround(-float(shift) / datamove * sbmove);

}

int calcsbshift( int pos, int viewsize, int datasize )
{
    if (viewsize >= datasize)
        return 0;

    float k = float(viewsize) / float(datasize);
    float sbmove = (float)(viewsize - minsbsize);
    float datamove = float(datasize - viewsize);
    int size = lround(ts::tmax<float>(k * viewsize, minsbsize));
    sbmove = float(viewsize - size);

    ASSERT(sbmove > 0);

    return lround(-float(pos) / sbmove * datamove);
}

void sbhelper_s::draw( const theme_rect_s *thr, rectengine_c &engine, evt_data_s &dd )
{
    calcsb(dd.draw_thr.sbpos, dd.draw_thr.sbsize, shift, dd.draw_thr.sbrect().height(), datasize);
    engine.draw(*thr, DTHRO_VSB, &dd);
    sbrect = dd.draw_thr.sbrect();
}

void text_remove_tags(ts::wstr_c &text)
{
    for (int i = text.find_pos('<'); i >= 0; i = text.find_pos(i, '<'))
    {
        int j = text.find_pos(i + 1, '>');
        if (j < 0)
        {
            text.set_length(i);
            break;
        }
        text.cut(i, j - i + 1);
    }
}

bool check_always_ok(const ts::wstr_c &)
{
    return true;
}
