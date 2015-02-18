#include "stdafx.h"

#define AUDIO_BITRATE 32000
#define AUDIO_SAMPLEDURATION 20

#define BROADCAST_PORT 0x8ee8
#define BROADCAST_RANGE 10

#define TCP_PORT 0x7ee7
#define TCP_RANGE 10

lan_engine::media_stuff_s::~media_stuff_s()
{
    if (audio_encoder)
        opus_encoder_destroy(audio_encoder);

    if (audio_decoder)
        opus_decoder_destroy(audio_decoder);
}

void lan_engine::media_stuff_s::init_audio_encoder()
{
    if (audio_encoder)
        opus_encoder_destroy(audio_encoder);

    int rc = OPUS_OK;
    audio_encoder = opus_encoder_create(AUDIO_SAMPLERATE, AUDIO_CHANNELS, OPUS_APPLICATION_VOIP, &rc);

    if (rc != OPUS_OK)
        return;

    rc = opus_encoder_ctl(audio_encoder, OPUS_SET_BITRATE(AUDIO_BITRATE));

    if (rc != OPUS_OK)
        return;

    rc = opus_encoder_ctl(audio_encoder, OPUS_SET_COMPLEXITY(10));

    if (rc != OPUS_OK)
        return;

}

void lan_engine::media_stuff_s::add_audio( const void *data, int datasize )
{
    if (audio_encoder == nullptr)
    {
        enc_fifo.clear();
        init_audio_encoder();
    }
    enc_fifo.add_data(data, datasize);
}

int lan_engine::media_stuff_s::encode_audio(byte *dest, int dest_max, const void *uncompressed_frame, int frame_size)
{
    if (audio_encoder)
        return opus_encode(audio_encoder, (opus_int16 *)uncompressed_frame, frame_size, dest, dest_max);

    return 0;
}

int lan_engine::media_stuff_s::prepare_audio4send()
{
    int avsize = enc_fifo.available();
    if (avsize == 0)
        return 0;

    int req_frame_size = audio_format_s(AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS).avgBytesPerMSecs(AUDIO_SAMPLEDURATION);
    if (req_frame_size > avsize)
        return 0;

    if (req_frame_size > (int)uncompressed.size()) uncompressed.resize(req_frame_size);
    int samples = enc_fifo.read_data(uncompressed.data(), req_frame_size) / audio_format_s(AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS).blockAlign();

    if (req_frame_size >(int)compressed.size()) compressed.resize(req_frame_size);
    return encode_audio(compressed.data(), req_frame_size, uncompressed.data(), samples);
}

int lan_engine::media_stuff_s::decode_audio( const void *data, int datasize )
{
    int rc;
    if (audio_decoder == nullptr)
    {
        audio_decoder = opus_decoder_create(AUDIO_SAMPLERATE, AUDIO_CHANNELS, &rc);
        if (rc != OPUS_OK) return 0;
    }

    int frames = AUDIO_SAMPLERATE * AUDIO_SAMPLEDURATION / 1000;
    int dec_size = frames * AUDIO_CHANNELS * AUDIO_BITS / 8;
    if ((int)uncompressed.size() < dec_size) uncompressed.resize(dec_size);

    rc = opus_decode(audio_decoder, (const byte *)data, datasize, (opus_int16 *)uncompressed.data(), frames, 0);
    if (rc > 0)
    {
        ASSERT( rc * AUDIO_CHANNELS * AUDIO_BITS / 8 == dec_size );
        return dec_size;
    }
    return 0;
}

void lan_engine::media_stuff_s::tick(contact_s *c)
{
    int sz = prepare_audio4send();
    if (sz > 0)
    {
        c->add_message(MTL_AUDIO_FRAME, now(), asptr((char *)compressed.data(), sz), 0);
    }
}


#if LOGGING
asptr pid_name(packet_id_e pid)
{
#define DESC_PID( p ) case p: return CONSTASTR( #p )
    switch (pid)
    {
        DESC_PID( PID_NONE );
        DESC_PID( PID_SEARCH );
        DESC_PID( PID_HALLO );
        DESC_PID( PID_MEET );
        DESC_PID( PID_NONCE );
        DESC_PID( PID_INVITE );
        DESC_PID( PID_ACCEPT );
        DESC_PID( PID_READY );
        DESC_PID( PID_MESSAGE );
        DESC_PID( PID_DELIVERED );
        DESC_PID( PID_REJECT );
    }
    return CONSTASTR("pid unknown");
}

void log_auth_key( const char *what, const byte *key )
{
    str_c nonce_part; nonce_part.append_as_hex(key, SIZE_KEY_NONCE_PART);
    str_c pass_part; pass_part.append_as_hex(key + SIZE_KEY_NONCE_PART, SIZE_KEY_PASS_PART);
    Log( "%s / %s - %s", nonce_part.cstr(), pass_part.cstr(), what );
}

void log_bytes( const char *what, const byte *b, int sz )
{
    str_c bs; bs.append_as_hex(b, sz);
    Log("%s - %s", bs.cstr(), what);
}

#define logm Log
#else
#define log_auth_key(a,b)
#define log_bytes(a, b, c)
#define logm(...)
#endif

void udp_sender::prepare()
{
    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    int val = 1;
    if (SOCKET_ERROR==setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&val, sizeof(val)))
        close();
}

void udp_sender::send(const void *data, int size, int portshift)
{
    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.S_un.S_addr = 0xFFFFFFFF; // BROADCAST
    dest_addr.sin_port = htons((unsigned short)(port + portshift));

    for(;;)
    {
        int r = sendto(_socket, (const char *)data, size, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));
        if (0 > r)
        {
            int error = WSAGetLastError();
            if (10054 == error) continue;
            if ((10049 == error || 10065 == error) /*&& ADDR_BROADCAST == dest_addr->sin_addr.S_un.S_addr*/) continue;
            close();
        };
        break;
    }

}

int udp_listner::open()
{
    sockaddr_in addr;

    for(int maxport = port + BROADCAST_RANGE;port < maxport;++port)
    {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (INVALID_SOCKET == _socket)
            continue;

        int val = 1;
        if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val)))
        {
            close();
            continue;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = 0;
        addr.sin_port = htons((unsigned short)port);

        if (SOCKET_ERROR == bind(_socket, (SOCKADDR*)&addr, sizeof(addr))){
            close();
            continue;
        };
        return port;
    }
    return -1;
}

int udp_listner::read( void *data, int datasize, sockaddr_in &addr )
{
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(_socket, &rs);
    timeval ti = {0,100};
    if (1 != select(0, &rs, nullptr, nullptr, &ti)) return 0;

    int sizeaddr = sizeof(sockaddr_in);

    for(;;)
    {
        int numr = recvfrom(_socket, (char *)data, datasize, 0, (sockaddr*)&addr, &sizeaddr);
        if (0 > numr)
        {
            int error = WSAGetLastError();
            if (10054 == error) continue;
            close();
        } else
            return numr;
        break;
    }

    return 0;
}

static void sacceptor(tcp_listner *me)
{
    sockaddr_in addr;

    int port = me->state.lock_read()().port;

    for (int maxport = port + BROADCAST_RANGE; port < maxport; ++port)
    {
        me->_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (INVALID_SOCKET == me->_socket)
            continue;

        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = 0;
        addr.sin_port = htons((unsigned short)port);

        if (SOCKET_ERROR == bind(me->_socket, (SOCKADDR*)&addr, sizeof(addr))){
            me->close();
            continue;
        };
        
        if (SOCKET_ERROR == listen(me->_socket, SOMAXCONN))
        {
            me->close();
            continue;
        }

        auto w = me->state.lock_write();
        w().port = port;
        w().acceptor_works = true;
        w.unlock();


        for(;!me->state.lock_read()().acceptor_should_stop;)
        {
            int AddrLen = sizeof(addr);
            SOCKET s = accept(me->_socket, (sockaddr*)&addr, &AddrLen);
            if (INVALID_SOCKET == s)
                break;
            me->accepted.push( new tcp_pipe(s, addr) );

        }

        break;
    }

    auto ww = me->state.lock_write();
    ww().acceptor_stoped = true;
    ww().acceptor_works = false;
}

DWORD WINAPI tcp_listner::acceptor(LPVOID p)
{
    UNSTABLE_CODE_PROLOG
    sacceptor((tcp_listner *)p);
    UNSTABLE_CODE_EPILOG
    return 0;
}

int tcp_listner::open()
{
    CloseHandle(CreateThread(nullptr, 0, acceptor, this, 0, nullptr));

    int port;
    for(;state.lock_read()().wait_acceptor_start(port); Sleep(0)) ;
    return port;
}

tcp_listner::~tcp_listner()
{
    state.lock_write()().acceptor_should_stop = true;
    close();
    for(;state.lock_read()().wait_acceptor_stop(); Sleep(0)) ;

    tcp_pipe *p;
    for( ; accepted.try_pop(p) ; )
        delete p;

}

bool tcp_pipe::connect()
{
    if (connected()) closesocket(_socket);
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == _socket)
        return false;

#if 0
    int val = 524288;
    if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&val, sizeof(val)))
    {
        close();
        return false;
    }
#endif
    if (SOCKET_ERROR == ::connect(_socket, (LPSOCKADDR)&addr, sizeof(addr)))
    {
        close();
        //error_ = WSAGetLastError();
        //if (WSAEWOULDBLOCK != error_)
            return false;
    }
    return true;
}

bool tcp_pipe::send( const byte *data, int datasize )
{
    const byte *d2s = data;
    int sent = 0;

    do
    {
        int iRetVal = ::send(_socket, (const char *)(d2s + sent), datasize - sent, 0);
        if (iRetVal == SOCKET_ERROR)
        {
            return false;
        }
        sent += iRetVal;
    } while (sent < datasize);

    return true;
}

packet_id_e tcp_pipe::packet_id()
{
    rcv_all();
    if ( rcvbuf_sz >= SIZE_PACKET_HEADER )
    {
        packet_id_e pid = (packet_id_e)ntohs(*(short *)rcvbuf);
        return pid;
    }
    return connected() ? PID_NONE : PID_DEAD;
}

void tcp_pipe::rcv_all()
{
    if (rcvbuf_sz >= sizeof(rcvbuf))
        return;

    for (;;)
    {
        fd_set rs;
        rs.fd_count = 1;
        rs.fd_array[0] = _socket;

        TIMEVAL tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        int n = ::select(0, &rs, nullptr, nullptr, &tv);
        if (n == 1)
        {
            int buf_free_space = sizeof(rcvbuf) - rcvbuf_sz;
            int reqread = 4096; if (buf_free_space < reqread) reqread = buf_free_space;
            
            int _bytes = ::recv(_socket, (char *)rcvbuf + rcvbuf_sz, reqread, 0);
            if (_bytes == 0 || _bytes == SOCKET_ERROR)
            {
                // connection closed
                close();
                return;
            }
            rcvbuf_sz += _bytes;
            continue;
        } else if(n == SOCKET_ERROR)
        {
            close();
        }

        break;
    }
}



void protect_raw_id( byte *raw_pub_id )
{
    for (int i = PUB_ID_INDEP_SIZE; i < SIZE_PUBID; ++i)
        raw_pub_id[i] = (byte)i;

    byte hash[crypto_shorthash_BYTES];
    static_assert(crypto_shorthash_KEYBYTES <= PUB_ID_INDEP_SIZE, "pub id too short");
    crypto_shorthash(hash, raw_pub_id, PUB_ID_INDEP_SIZE, raw_pub_id + SIZE_PUBID - crypto_shorthash_KEYBYTES);
    memcpy(raw_pub_id + PUB_ID_INDEP_SIZE, hash, PUB_ID_CHECKSUM_SIZE);
}

bool check_pubid_valid(const byte *raw_pub_id_i)
{
    byte raw_pub_id[SIZE_PUBID];
    memcpy(raw_pub_id,raw_pub_id_i, PUB_ID_INDEP_SIZE);
    protect_raw_id(raw_pub_id);
    return 0 == memcmp(raw_pub_id+PUB_ID_INDEP_SIZE, raw_pub_id_i+PUB_ID_INDEP_SIZE,PUB_ID_CHECKSUM_SIZE);
}

bool make_raw_pub_id( byte *raw_pub_id, const asptr &pub_id )
{
    if (ASSERT(pub_id.l == SIZE_PUBID * 2))
    {
        pstr_c p(pub_id);
        for (int i = 0; i < SIZE_PUBID; ++i)
            raw_pub_id[i] = p.as_byte_hex(i * 2);
        return true;
    }
    return false;
}

bool check_pubid_valid(const asptr &pub_id)
{
    byte raw_pub_id[SIZE_PUBID];
    if (make_raw_pub_id(raw_pub_id, pub_id))
        return check_pubid_valid(raw_pub_id);
    return false;
}

void make_raw_pub_id( byte *raw_pub_id, const byte *pk )
{
    static_assert(crypto_generichash_BYTES_MIN >= PUB_ID_INDEP_SIZE, "pub id too short");

    crypto_generichash(raw_pub_id, PUB_ID_INDEP_SIZE, pk, SIZE_PUBLIC_KEY, nullptr, 0);
    protect_raw_id(raw_pub_id);
}

msg_s *msg_s::build(message_type_lan_e mt, u64 create_time, const asptr&text, u64 utag)
{
    msg_s * m = (msg_s *)dlmalloc( sizeof(msg_s) + text.l );
    m->create_time = create_time;
    m->delivery_tag = utag;
    m->next = nullptr;
    m->prev = nullptr;
    m->mt = mt;
    m->sent = 0;
    m->len = text.l;
    memcpy( m+1, text.s, text.l );
    return m;
}
void msg_s::die()
{
    dlfree(this);
}

lan_engine::contact_s::~contact_s()
{
    if (media)
        delete media;
    for(;sendmessage_f;)
    {
        msg_s *m = sendmessage_f;
        LIST_DEL(m,sendmessage_f,sendmessage_l,prev,next);
        m->die();
    }
}

void lan_engine::contact_s::add_message(message_type_lan_e mt, u64 create_time, asptr text, u64 utag)
{
    char zero = 0;
    if (text.l == 0)
    {
        text.s = &zero;
        text.l = 1;
    }

    if (utag == 0) do {
        randombytes_buf(&utag, sizeof(utag));
    } while (delivery.find(utag) != delivery.end());


    msg_s *m = msg_s::build(mt, create_time, text,utag);
    LIST_ADD(m,sendmessage_f,sendmessage_l,prev,next);
    if (state == ONLINE)
        nextactiontime = time_ms();
}


void lan_engine::contact_s::calculate_pub_id( const byte *pk )
{
    memcpy(public_key, pk, SIZE_PUBLIC_KEY);
    make_raw_pub_id(raw_public_id, pk);
    public_id.clear().append_as_hex(raw_public_id, SIZE_PUBID);
}


void lan_engine::setup_communication_stuff()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    broadcast_trap.open();
    broadcast_seek.prepare();
    listen_port = tcp_in.open();
}

void lan_engine::shutdown_communication_stuff()
{
    tcp_in.close();
    broadcast_seek.close();
    broadcast_trap.close();
    WSACleanup();
}

lan_engine *lan_engine::engine = nullptr;
void lan_engine::create(host_functions_s *hf)
{
#pragma warning( disable: 4316 ) // 'lan_engine' : object allocated on the heap may not be aligned 16
    if (ASSERT(engine == nullptr))
        engine = new lan_engine(hf);
}

bool set_cfg_called = false;

lan_engine::lan_engine( host_functions_s *hf ):hf(hf),broadcast_trap(BROADCAST_PORT), broadcast_seek(BROADCAST_PORT), tcp_in(TCP_PORT)
{
    set_cfg_called = false;
    lasthallo = time_ms() - 20000;
    //last_message = lasthallo;
    addnew()->state = contact_s::OFFLINE; /* create me */ 
};

lan_engine::~lan_engine()
{
    while (first)
        del(first);

    shutdown_communication_stuff();
}


void lan_engine::tick()
{
    if (first->state != contact_s::ONLINE || listen_port < 0) return;

    contact_s *rotten = nullptr;
    bool need_some_hallo = false;
    int ct = time_ms();
    for (contact_s *c = first->next; c; c=c->next)
    {
        if (rotten == nullptr && c->state == contact_s::ROTTEN)
            rotten = c;

        c->changed_self |= changed_some;

        int deltat = (int)(ct - c->nextactiontime);
        if (deltat >= 0)
        {
            switch (c->state)
            {
            case contact_s::SEARCH:
                
                pg_search(listen_port, c->raw_public_id);
                broadcast_seek.send(packet_buf_encoded, packet_buf_encoded_len, c->portshift);
                ++c->portshift;
                if (c->portshift >= BROADCAST_RANGE)
                {
                    c->nextactiontime = ct + 5000;
                    c->portshift = 0;
                } else
                    c->nextactiontime = ct + 100;
                break;
            case contact_s::TRAPPED:
            case contact_s::ACCEPT_RESTORE_CONNECT:
                if (c->pipe.connected())
                {
                    if (c->key_sent)
                    {
                        // oops. no invite still received
                        // it means no valid connection
                        c->pipe.close(); // goodbye connection

                    } else
                    {
                        pg_meet(c->public_key, c->temporary_key);
                        bool ok = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                        if (ok)
                        {
                            c->key_sent = true;
                            c->nextactiontime = ct + 2000;
                        } else
                            c->pipe.close();
                    }

                } else
                {
                    if (c->key_sent)
                    {
                        ++c->reconnect;
                        c->key_sent = false;

                        if (c->reconnect > 10)
                        {
                            c->state = contact_s::ROTTEN;
                            if (rotten == nullptr) rotten = c;
                        }
                    }
                    if (c->state != contact_s::ROTTEN)
                    {
                        c->pipe.connect();
                        c->nextactiontime = ct + 2000;
                    }
                }
                break;
            case contact_s::MEET:
                if (c->pipe.connected())
                {
                    pg_invite(first->name, c->invitemessage, c->temporary_key);
                    bool ok = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                    if (ok)
                    {
                        c->state = contact_s::INVITE_SEND;
                        c->data_changed = true;
                    } else
                    {
                        c->pipe.close();
                        c->nextactiontime = ct;
                    }

                } else
                {
                    c->state = contact_s::SEARCH;
                    c->nextactiontime = ct;
                }
                break;
            case contact_s::BACKCONNECT:
                if (c->pipe.connected())
                {
                    if (c->key_sent)
                    {
                        c->pipe.close(); 
                    } else
                    {
                        randombytes_buf(c->authorized_key, SIZE_KEY_NONCE_PART); // rebuild nonce

                        pg_nonce(c->public_key, c->authorized_key);
                        bool ok = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                        if (ok)
                        {
                            logm("pg_nonce send ok");

                            c->key_sent = true;
                            c->nextactiontime = ct + 5000; // waiting PID_READY in 5 seconds, then disconnect
                        } else
                        {
                            logm("pg_nonce send fail, close pipe");

                            c->pipe.close();
                            c->nextactiontime = ct;
                        }
                    }
                } else
                {
                    if (c->key_sent)
                    {
                        ++c->reconnect;
                        c->key_sent = false;

                        if (c->reconnect > 10)
                        {
                            c->state = contact_s::OFFLINE;
                        }
                    }

                    if (c->state != contact_s::OFFLINE)
                    {
                        logm("pg_nonce connect");

                        // just connect
                        c->pipe.connect();
                        c->nextactiontime = ct + 2000;
                    }
                }
                break;
            case contact_s::INVITE_SEND:
                if (c->pipe.connected())
                {
                    // do nothing. waiting accept or reject
                    c->online_tick(ct, 1100);
                } else
                {
                    // connection lost :(
                    c->state = contact_s::SEARCH;
                    c->nextactiontime = ct;
                }

                break;
            case contact_s::INVITE_RECV:
                if (c->pipe.connected())
                {
                    c->online_tick(ct, 1100);
                }
                break;
            case contact_s::ACCEPT:
                // send accept and kill contact
                if (c->pipe.connected())
                {
                    if (!c->key_sent)
                    {
                        randombytes_buf(c->authorized_key, sizeof(c->authorized_key));

                        log_auth_key( "before send accept", c->authorized_key );

                        pg_accept(first->name, c->authorized_key, c->temporary_key);
                        c->key_sent = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                    }
                    c->nextactiontime = ct + 5000;
                } else
                {
                    // just wait
                    c->nextactiontime = ct + 10000;
                    c->key_sent = false;
                }
                break;
            case contact_s::REJECT:
                // send reject and kill contact
                if (c->pipe.connected())
                {
                    pg_reject();
                    if (c->pipe.send(packet_buf_encoded, packet_buf_encoded_len))
                        c->pipe.flush_and_close();
                }

                c->state = contact_s::ROTTEN;
                c->nextactiontime = ct;
                if (rotten == nullptr) rotten = c;
                c->data_changed = true;
                break;
            case contact_s::ONLINE:
                if (c->pipe.connected())
                {
                    c->online_tick(ct);

                } else if ( (ct - c->nextactiontime) > 10000 )
                {
                    c->state = contact_s::OFFLINE;
                    c->data_changed = true;
                }
                break;
            }
            
        }

        if (c->state == contact_s::OFFLINE)
        {
            if (!c->pipe.connected())
                need_some_hallo = true;
        }

        c->recv();

        if (!c->pipe.connected())
        {
            if (c->state == contact_s::ONLINE)
            {
                c->state = contact_s::OFFLINE;
                c->data_changed = true;
                c->correct_create_time = 0;
                c->need_resync = false;
            }
        }
        
        if (need_some_hallo)
        {
            int deltat = (int)(ct - lasthallo);
            if (deltat > 10000)
            {
                lasthallo = ct;
                // one hallo per 10 sec

                pg_hallo(listen_port);
                for(int i=0;i<BROADCAST_RANGE;++i)
                    broadcast_seek.send(packet_buf_encoded, packet_buf_encoded_len, i);

            }
        }
    }

    changed_some = 0;

    if (rotten)
    {
        ASSERT(!rotten->data_changed, "sure \'rotten\' state was sent to host");
        del(rotten);
    }

    if (first->data_changed) // send self status
    {
        contact_data_s cd;
        first->fill_data(cd);
        hf->update_contact(&cd);
    }

    // broadcast receive
    sockaddr_in addr;
    packet_buf_encoded_len = broadcast_trap.read(packet_buf_encoded,512,addr);
    if ( packet_buf_encoded_len >= 2 )
    {
        decode();

        stream_reader reader(2, packet_buf, packet_buf_len);

        packet_id_e pid = (packet_id_e)ntohs(*(short *)packet_buf);
        switch (pid)
        {
            case PID_SEARCH:
            {
                USHORT version = reader.readus(0);
                if (version != LAN_PROTOCOL_VERSION)
                    break;
                USHORT back_port = reader.readus(0);
                const byte *trapped_contact_public_key = reader.read(SIZE_PUBLIC_KEY);
                const byte *seeking_raw_public_id = reader.read(SIZE_PUBID);
                if (ASSERT(trapped_contact_public_key && seeking_raw_public_id && reader.last() == 0))
                    pp_search( addr.sin_addr.S_un.S_addr, back_port, trapped_contact_public_key, seeking_raw_public_id );
                break;
            }
            case PID_HALLO:
            {
                USHORT version = reader.readus(0);
                if (version != LAN_PROTOCOL_VERSION)
                    break;
                USHORT back_port = reader.readus(0);
                const byte *hallo_contact_public_key = reader.read(SIZE_PUBLIC_KEY);
                if (ASSERT(hallo_contact_public_key && reader.last() == 0))
                    pp_hallo(addr.sin_addr.S_un.S_addr, back_port, hallo_contact_public_key);
                break;
            }
        }
    }

    // incoming connection
    tcp_pipe *pipe = nullptr;
    if (tcp_in.accepted.try_pop(pipe))
    {
        switch (pipe->packet_id())
        {
        case PID_MEET:
            pipe = pp_meet(pipe, stream_reader(pipe->rcvbuf, pipe->rcvbuf_sz));
            break;
        case PID_NONCE:
            pipe = pp_nonce(pipe, stream_reader(pipe->rcvbuf, pipe->rcvbuf_sz));
            break;
        case PID_NONE:
            // not yet received
            if (pipe->timeout()) break;

            tcp_in.accepted.push(pipe); // push to queue
            pipe = nullptr;
            break;
        }

        if (pipe) delete pipe;
    }
}

void lan_engine::pp_search( unsigned int IPv4, int back_port, const byte *trapped_contact_public_key, const byte *seeking_raw_public_id )
{
    if ( 0 != memcmp(seeking_raw_public_id, first->raw_public_id, SIZE_PUBID) ) return; // do nothin
    contact_s *c = find_by_pk( trapped_contact_public_key ); // may be already trapped
    if (!c) c = addnew(); // no. 1st search
    else if (c->pipe.connected()) return; // already connected - ignore. we think it is parasite packet
    if (c->state == contact_s::ACCEPT)
        c->state = contact_s::ACCEPT_RESTORE_CONNECT;
    else
        c->state = contact_s::TRAPPED;
    c->key_sent = false;
    c->reconnect = 0;
    c->calculate_pub_id(trapped_contact_public_key);
    c->pipe.set_address(IPv4, back_port);
    randombytes_buf(c->temporary_key, SIZE_KEY);
}

void lan_engine::pp_hallo( unsigned int IPv4, int back_port, const byte *hallo_contact_public_key )
{
    if (contact_s *c = find_by_pk( hallo_contact_public_key ))
    {
        // yo! this guy is in my contact list!
        if (!c->pipe.connected() && c->state == contact_s::OFFLINE)
        {
            // only offline (authorized) contacts can be initialized with PID_HALLO packet
            c->state = contact_s::BACKCONNECT;
            c->key_sent = false;
            c->reconnect = 0;
            c->pipe.set_address(IPv4, back_port);
            c->reconnect = 0;
            c->nextactiontime = time_ms();
        }
    }
}

tcp_pipe * lan_engine::pp_meet( tcp_pipe * pipe, stream_reader &&r )
{
    if (r.end())
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const byte *meet_public_key = r.read(SIZE_PUBLIC_KEY);
    if (!meet_public_key)
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    byte pubid[SIZE_PUBID];
    make_raw_pub_id(pubid, meet_public_key);

    contact_s *c = find_by_rpid(pubid);
    if (c == nullptr || (c->state != contact_s::SEARCH && c->state != contact_s::INVITE_SEND))
    {
        // only way to handle this packet - send broadcast udp PID_SEARCH packet and get incoming tcp connection
        // so, no client, no PID_SEARCH
        return pipe;
    }


    const byte *nonce = r.read(crypto_box_NONCEBYTES);
    if (!nonce) 
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const int cipher_len = SIZE_KEY + crypto_box_MACBYTES;

    const byte *cipher = r.read(cipher_len);
    if (!cipher)
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const int decoded_size = cipher_len - crypto_box_MACBYTES;
    static_assert(decoded_size == SIZE_KEY, "check size");

    if (crypto_box_open_easy(c->temporary_key, cipher, cipher_len, nonce, meet_public_key, my_secret_key) != 0)
        return pipe;

    memcpy(c->public_key, meet_public_key, SIZE_PUBLIC_KEY);
    c->pipe = std::move( *pipe );
    c->state = contact_s::MEET;
    c->nextactiontime = time_ms();
    c->pipe.cpdone();

    return pipe;
}

tcp_pipe *lan_engine::pp_nonce(tcp_pipe * pipe, stream_reader &&r)
{
    logm("pp_nonce (rcvd) ==============================================================");
    log_bytes("my_public_key", my_public_key, SIZE_PUBLIC_KEY);

    if (r.end())
    {
        logm("pp_nonce no data");
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const byte *peer_public_key = r.read(SIZE_PUBLIC_KEY);
    if (!peer_public_key)
    {
        logm("pp_nonce no peer_public_key");
        tcp_in.accepted.push(pipe);
        return nullptr;
    }
   
    log_bytes("peer_public_key (rcvd)", peer_public_key, SIZE_PUBLIC_KEY);

    byte pubid[SIZE_PUBID];
    make_raw_pub_id(pubid, peer_public_key);

    contact_s *c = find_by_rpid(pubid);
    while (c == nullptr || (c->state != contact_s::OFFLINE))
    {
        if (c && c->state == contact_s::BACKCONNECT)
        {
            // concurrent connection
            // one of contacts should ignore incoming connection, but other should continue
            // compare public keys to decide who should drop incoming connection
            if (memcmp( my_public_key, c->public_key, SIZE_PUBLIC_KEY ) < 0)
                break;

            logm("concurrent fail");

        } else
        {
            logm(c ? "found non-offline contact" : "no such key");
        }

        // only way to handle this packet - send broadcast udp PID_HALLO packet and get incoming tcp connection
        // so, no client, no PID_HALLO
        return pipe;
    }

    logm("concurrent ok");
    log_auth_key("c auth_key", c->authorized_key);
    log_bytes("c public_key", c->public_key, SIZE_PUBLIC_KEY);

    const byte *nonce = r.read(crypto_box_NONCEBYTES);
    if (!nonce)
    {
        logm("no nonce yet");
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    log_bytes("nonce (rcvd)", nonce, crypto_box_NONCEBYTES);

    const int cipher_len = SIZE_KEY_NONCE_PART + crypto_box_MACBYTES;

    const byte *cipher = r.read(cipher_len);
    if (!cipher)
    {
        logm("no cipher yet");
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const int decoded_size = cipher_len - crypto_box_MACBYTES;
    static_assert(decoded_size == SIZE_KEY_NONCE_PART, "check size");

    if (crypto_box_open_easy(c->authorized_key, cipher, cipher_len, nonce, peer_public_key, my_secret_key) != 0)
    {
        logm("cipher cannot be decrypted");
        return pipe;
    }

    log_auth_key( "authkey decrypted", c->authorized_key );

    const byte *rhash = r.read(crypto_generichash_BYTES);
    if (!rhash)
    {
        logm("no hash yet");
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    log_bytes("hash (rcvd)", rhash, crypto_generichash_BYTES);

    byte hash[crypto_generichash_BYTES];
    crypto_generichash(hash, sizeof(hash), c->authorized_key, SIZE_KEY, nullptr, 0);

    log_bytes("hash of authkey", hash, crypto_generichash_BYTES);

    if ( 0 == memcmp(hash, rhash, crypto_generichash_BYTES) )
    {
        if (ASSERT(0 == memcmp(c->public_key, peer_public_key, SIZE_PUBLIC_KEY)))
        {
            c->pipe = std::move(*pipe);
            c->state = contact_s::ONLINE;
            c->data_changed = true;
            c->nextactiontime = time_ms();
            c->pipe.cpdone();
            c->changed_self = -1;

            pg_ready(c->raw_public_id, c->authorized_key);
            c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
        }
    } else
    {
        // authorized_key is damaged
        c->invitemessage = CONSTASTR("\1restorekey");
        c->state = contact_s::SEARCH;
        c->data_changed = true;
    }


    return pipe;
}

void lan_engine::goodbye()
{
    set_cfg_called = false;
    delete this;
    engine = nullptr;
}
void lan_engine::set_name(const char*utf8name)
{
    first->name = utf8name;
    changed_some |= CDM_NAME;
}
void lan_engine::set_statusmsg(const char*utf8status)
{
    first->statusmsg = utf8status;
    changed_some |= CDM_STATUSMSG;
}

void lan_engine::set_ostate(int ostate)
{
    first->ostate = (contact_online_state_e)ostate;
    changed_some |= CDM_ONLINE_STATE;
}
void lan_engine::set_gender(int gender)
{
    first->gender = (contact_gender_e)gender;
    changed_some |= CDM_GENDER;
}

enum chunks_e // HADR ORDER!!!!!!!!
{
    chunk_secret_key,
    chunk_contacts,

    chunk_contact,

    chunk_contact_id,
    chunk_contact_public_key,
    chunk_contact_name,
    chunk_contact_statusmsg,
    chunk_contact_state,
    chunk_contact_changedflags,

    chunk_contact_invitemsg,
    chunk_contact_tmpkey,
    chunk_contact_authkey,
    chunk_contact_sendmessages,

    chunk_contact_sendmessage,
    chunk_contact_sendmessage_type,
    chunk_contact_sendmessage_body,
    chunk_contact_sendmessage_utag,
    chunk_contact_sendmessage_createtime,

};

void lan_engine::set_config(const void*data, int isz)
{
    bool loaded = false;
    set_cfg_called = true;

    loader ldr(data,isz);

    if (int sz = ldr(chunk_secret_key))
    {
        loader l( ldr.chunkdata(), sz );
        int dsz;
        if (const void *sk = l.get_data(dsz))
            if (ASSERT(dsz == SIZE_SECRET_KEY))
                memcpy( my_secret_key, sk, SIZE_SECRET_KEY );
    }
    if (int sz = ldr(chunk_contacts))
    {
        while (first)
            del(first);

        loader l( ldr.chunkdata(), sz );
        for(int cnt = l.read_list_size();cnt > 0;--cnt)
            if (int contact_size = l(chunk_contact))
            {
                contact_s *c = nullptr;

                loader lc(l.chunkdata(), contact_size);
                if (lc(chunk_contact_id))
                    c = addnew( lc.get_int() );
                if (c)
                {
                    if (int pksz = lc(chunk_contact_public_key))
                    {
                        loader pkl(lc.chunkdata(), pksz);
                        int dsz;
                        if (const void *pk = pkl.get_data(dsz))
                            if (ASSERT(dsz == SIZE_PUBLIC_KEY))
                            {
                                loaded = true;
                                c->calculate_pub_id((const byte *)pk);
                                if (c->id == 0) memcpy( my_public_key, c->public_key, SIZE_PUBLIC_KEY );
                            }
                    }
                    if (lc(chunk_contact_name))
                        c->name = lc.get_astr();

                    if (lc(chunk_contact_statusmsg))
                        c->statusmsg = lc.get_astr();

                    if (lc(chunk_contact_state))
                        c->state = (decltype(c->state))lc.get_int();

                    if (c->state == contact_s::SEARCH && c->id == 0)
                        c->state = contact_s::OFFLINE;

                    if (c->state == contact_s::ONLINE || c->state == contact_s::BACKCONNECT)
                        c->state = contact_s::OFFLINE;

                    if (c->state == contact_s::INVITE_SEND || c->state == contact_s::MEET)
                        c->state = contact_s::SEARCH; // if contact not yet authorized - initial search is only valid way to establish connection

                    if (c->state == lan_engine::contact_s::SEARCH
                        || c->state == lan_engine::contact_s::MEET
                        || c->state == lan_engine::contact_s::INVITE_SEND
                        || c->state == lan_engine::contact_s::INVITE_RECV)
                        if (lc(chunk_contact_invitemsg))
                            c->invitemessage = lc.get_astr();

                    //if (c->invitemessage.is_empty() && c->state == lan_engine::contact_s::SEARCH)
                    //    c->invitemessage = CONSTASTR("\1restorekey");

                    if (c->state != lan_engine::contact_s::ONLINE && c->state != lan_engine::contact_s::OFFLINE)
                    {
                        if (int tksz = lc(chunk_contact_tmpkey))
                        {
                            loader tkl(lc.chunkdata(), tksz);
                            int dsz;
                            if (const void *tk = tkl.get_data(dsz))
                                if (ASSERT(dsz == SIZE_KEY))
                                    memcpy( c->temporary_key, tk, SIZE_KEY );
                        }

                    } else if (c->id)
                    {
                        randombytes_buf(c->authorized_key, sizeof (c->authorized_key));

                        if (int tksz = lc(chunk_contact_authkey))
                        {
                            loader tkl(lc.chunkdata(), tksz);
                            int dsz;
                            if (const void *tk = tkl.get_data(dsz))
                                if (ASSERT(dsz == SIZE_KEY_PASS_PART))
                                    memcpy(c->authorized_key + SIZE_KEY_NONCE_PART, tk, SIZE_KEY_PASS_PART);
                        }

                        log_auth_key( "loaded", c->authorized_key );
                    }

                    if (int sz = lc(chunk_contact_sendmessages))
                    {
                        while (msg_s *m = c->sendmessage_f)
                        {
                            LIST_DEL(m, c->sendmessage_f, c->sendmessage_l, prev, next);
                            m->die();
                        }

                        loader l(lc.chunkdata(), sz);
                        for (int cnt = l.read_list_size(); cnt > 0; --cnt)
                            if (int msgs_size = l(chunk_contact_sendmessage))
                            {
                                loader lcm(l.chunkdata(), msgs_size);
                                if ( lcm(chunk_contact_sendmessage_type) )
                                {
                                    message_type_lan_e mt = (message_type_lan_e)lcm.get_int();
                                    u64 create_time = now();
                                    u64 utag = 0;
                                    if (lcm(chunk_contact_sendmessage_utag))
                                        utag = lcm.get_u64();
                                    if (lcm(chunk_contact_sendmessage_createtime))
                                        create_time = lcm.get_u64();
                                    if (int mbsz = lcm(chunk_contact_sendmessage_body))
                                    {
                                        loader mbl(lcm.chunkdata(), mbsz);
                                        int dsz;
                                        if (const void *mb = mbl.get_data(dsz))
                                            c->add_message(mt, create_time, asptr((const char *)mb, dsz), utag);
                                    }
                                }
                            }
                    }

                    if (lc(chunk_contact_changedflags))
                        c->changed_self = lc.get_int();

                }
            }
    }

    if (!loaded)
    {
        // setup default

        crypto_box_keypair(my_public_key, my_secret_key);
        first->calculate_pub_id( my_public_key );
    }
}

lan_engine::contact_s *contact;

void operator<<(chunk &chunkm, const lan_engine::contact_s &c)
{
    if (c.state == lan_engine::contact_s::ROTTEN) return;
    contact = const_cast<lan_engine::contact_s *>(&c);

    chunk cc(chunkm.b, chunk_contact);

    chunk(chunkm.b, chunk_contact_id) << c.id;
    chunk(chunkm.b, chunk_contact_public_key) << bytes(c.public_key, SIZE_PUBLIC_KEY);
    chunk(chunkm.b, chunk_contact_name) << c.name;
    chunk(chunkm.b, chunk_contact_statusmsg) << c.statusmsg;
    chunk(chunkm.b, chunk_contact_state) << (int)c.state;
    

    if (c.state == lan_engine::contact_s::SEARCH
        || c.state == lan_engine::contact_s::MEET
        || c.state == lan_engine::contact_s::INVITE_SEND
        || c.state == lan_engine::contact_s::INVITE_RECV)
    {
        chunk(chunkm.b, chunk_contact_invitemsg) << c.invitemessage;
    }

    if (c.state != lan_engine::contact_s::ONLINE && c.state != lan_engine::contact_s::OFFLINE && c.state != lan_engine::contact_s::BACKCONNECT)
    {
        chunk(chunkm.b, chunk_contact_tmpkey) << bytes(c.temporary_key, SIZE_KEY);

    } else if (c.id)
    {
        chunk(chunkm.b, chunk_contact_authkey) << bytes(c.authorized_key + SIZE_KEY_NONCE_PART, SIZE_KEY_PASS_PART);

        log_auth_key( "saved", c.authorized_key );
    }

    if (c.sendmessage_f)
        chunk(chunkm.b, chunk_contact_sendmessages) << serlist<msg_s>(c.sendmessage_f);

    chunk(chunkm.b, chunk_contact_changedflags) << (int)c.changed_self;

    for (msg_s *m = c.sendmessage_f; m; )
    {
        msg_s *x = m; m = m->next;
        if (x->mt > __mtl_no_need_ater_save_begin && x->mt < __mtl_no_need_ater_save_end)
        {
            LIST_DEL(x, contact->sendmessage_f, contact->sendmessage_l, prev, next);
            x->die();
        }
    }

}
void operator<<(chunk &chunkm, const msg_s &m)
{
    // some messages here means not all changed values were delivered
    if (m.mt > __mtl_no_save_begin && m.mt < __mtl_no_save_end)
    {
        switch (m.mt)
        {
            case MTL_CHANGED_NAME: { contact->changed_self |= CDM_NAME; return; }
            case MTL_CHANGED_STATUSMSG: { contact->changed_self |= CDM_STATUSMSG; return; }
            case MTL_OSTATE: { contact->changed_self |= CDM_ONLINE_STATE; return; }
            case MTL_GENDER: { contact->changed_self |= CDM_GENDER; return; }
        }
        return;
    }

    chunk mm(chunkm.b, chunk_contact_sendmessage);

    chunk(chunkm.b, chunk_contact_sendmessage_type) << (int)m.mt;
    chunk(chunkm.b, chunk_contact_sendmessage_utag) << m.delivery_tag;
    chunk(chunkm.b, chunk_contact_sendmessage_createtime) << m.create_time;
    chunk(chunkm.b, chunk_contact_sendmessage_body) << bytes(m.text().s, m.len);
}

void lan_engine::save_config(void *param)
{
    if (engine && set_cfg_called)
    {
        savebuffer b;
        chunk(b, chunk_secret_key) << bytes(my_secret_key, SIZE_SECRET_KEY);
        chunk(b, chunk_contacts) << serlist<contact_s>(first);
        hf->on_save(b.data(), b.size(), param);
    }
}

void lan_engine::init_done()
{
    // send contacts to host
    contact_data_s cd;
    for(contact_s *c = first; c; c=c->next)
    {
        c->fill_data(cd);
        hf->update_contact(&cd);
    }
}
void lan_engine::online()
{
    if (first->state == contact_s::OFFLINE)
        setup_communication_stuff();

    if (broadcast_trap.ready())
    {
        first->state = contact_s::ONLINE;
        first->data_changed = true;
    }
}

void lan_engine::offline()
{
    if (first->state == contact_s::ONLINE)
    {
        first->state = contact_s::OFFLINE;

        contact_data_s cd;
        first->fill_data(cd);
        hf->update_contact(&cd);

        shutdown_communication_stuff();
    }
}

int lan_engine::resend_request(int id, const char* invite_message_utf8)
{
    if (contact_s *c = find(id))
        return add_contact( c->public_id, invite_message_utf8 );
    return CR_FUNCTION_NOT_FOUND;
}

int lan_engine::add_contact(const char* public_id, const char* invite_message_utf8)
{
    contact_data_s cd;

    str_c pubid( public_id );
    byte raw_pub_id[SIZE_PUBID];
    make_raw_pub_id(raw_pub_id,pubid.as_sptr());
    if (!check_pubid_valid(raw_pub_id))
        return CR_INVALID_PUB_ID;

    contact_s *c = find_by_rpid(raw_pub_id);
    if (c) 
    {
        if (c->state != contact_s::REJECTED && c->state != contact_s::INVITE_SEND)
            return CR_ALREADY_PRESENT;
    } else
    {
        c = addnew();
        c->public_id = pubid;
        make_raw_pub_id(c->raw_public_id, c->public_id);
    }
    c->invitemessage = asptr(invite_message_utf8);
    c->state = contact_s::SEARCH;
    if (c->pipe.connected()) c->pipe.close();

    c->fill_data(cd);
    hf->update_contact(&cd);
    return CR_OK;
}

void lan_engine::del_contact(int id)
{
    if (id)
        if (contact_s *c = find(id))
        {
            c->state = contact_s::ROTTEN;
            c->data_changed = false; // no need to send data to host
        }

}


lan_engine::contact_s *lan_engine::find_by_pk(const byte *public_key)
{
    for (contact_s *i = first->next; i; i = i->next)
        if (i->state != contact_s::ROTTEN)
            if (0 ==memcmp(i->public_key, public_key, SIZE_PUBLIC_KEY))
                return i;
    return nullptr;
}

lan_engine::contact_s *lan_engine::find_by_rpid(const byte *raw_public_id)
{
    for (contact_s *i = first->next; i; i = i->next)
        if (i->state != contact_s::ROTTEN)
            if (0 == memcmp(i->raw_public_id, raw_public_id, SIZE_PUBID))
                return i;
    return nullptr;
}

lan_engine::contact_s *lan_engine::addnew(int iid)
{
    int id = iid;
    if (id == 0)
        for(;find(id) != nullptr;++id) ; // slooooow. but fast enough on current cpus
    contact_s *nc = new contact_s( id );
    LIST_ADD(nc,first,last,prev,next);
    return nc;
}

void lan_engine::del(contact_s *c)
{
    LIST_DEL(c, first, last, prev, next);
    delete c;
}

void lan_engine::send(int id, const message_s *msg)
{
    if (contact_s *c = find(id))
        c->add_message((message_type_lan_e)msg->mt, now(), asptr(msg->message, msg->message_len), msg->utag);
}

void lan_engine::accept(int id)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::INVITE_RECV)
        {
            c->state = contact_s::ACCEPT;
            c->key_sent = false;
            c->invitemessage.clear();
        }
}

void lan_engine::reject(int id)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::INVITE_RECV)
            c->state = contact_s::REJECT;
}

void lan_engine::call(int id, const call_info_s *callinf)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::ONLINE && c->call_status == contact_s::CALL_OFF)
        {
            c->add_message(MTL_CALL, now(), asptr(), 0);
            c->call_status = contact_s::OUT_CALL;
            c->call_stop_time = time_ms() + (callinf->duration * 1000);
        }
}

void lan_engine::stop_call(int id, stop_call_e /*scr*/)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::ONLINE && contact_s::CALL_OFF != c->call_status)
        {
            c->add_message(MTL_CALL_CANCEL, now(), asptr(), 0);
            c->stop_call_activity(false);
            
        }
}

void lan_engine::accept_call(int id)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::ONLINE && contact_s::IN_CALL == c->call_status)
        {
            c->add_message(MTL_CALL_ACCEPT, now(), asptr(), 0);
            c->start_media();
        }
}

void lan_engine::send_audio(int id, const call_info_s * ci)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::ONLINE && contact_s::IN_PROGRESS == c->call_status && c->media)
            c->media->add_audio( ci->audio_data, ci->audio_data_size );
}


bool decrypt( byte *outbuf, const byte* inbuf, int inbufsz, const byte *tmpkey )
{
    //int decrypt_len = inbufsz - crypto_secretbox_MACBYTES;
    *(USHORT *)outbuf = *(USHORT *)inbuf;
    USHORT datasz = ntohs(*(USHORT *)(inbuf+2));
    *(USHORT *)(outbuf + 2) = (USHORT)htons( datasz - crypto_secretbox_MACBYTES );

    if (datasz > inbufsz) return false; // not all data received

    if (crypto_secretbox_open_easy(outbuf + SIZE_PACKET_HEADER, inbuf + SIZE_PACKET_HEADER, datasz-SIZE_PACKET_HEADER, tmpkey, tmpkey + crypto_secretbox_NONCEBYTES) != 0)
        return false;

    return true;
}

void lan_engine::contact_s::online_tick(int ct, int nexttime)
{
    if (state == ONLINE)
    {
        if (need_resync && (ct-next_sync)>0)
        {
            engine->pg_time(true, authorized_key);
            pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
            next_sync = ct + 120000;
        }


        if (changed_self)
        {
            time_t n = now();
            if (0 != (changed_self & CDM_NAME))
                add_message(MTL_CHANGED_NAME, n, engine->first->name, 0);

            if (0 != (changed_self & CDM_STATUSMSG))
                add_message(MTL_CHANGED_STATUSMSG, n, engine->first->statusmsg, 0);

            if (0 != (changed_self & CDM_ONLINE_STATE))
                add_message(MTL_OSTATE, n, str_c().set_as_int(engine->first->ostate), 0);

            if (0 != (changed_self & CDM_GENDER))
                add_message(MTL_GENDER, n, str_c().set_as_int(engine->first->gender), 0);

            changed_self = 0;
        }

        if (call_status == OUT_CALL)
        {
            int deltat = (int)(ct - call_stop_time);
            if (deltat >= 0)
            {
                add_message(MTL_CALL_CANCEL, now(), asptr(), 0);
                stop_call_activity(true);
            }
        }
    }

    if (media) media->tick( this );

    msg_s *m = sendmessage_f;
    while (m && m->sent >= m->len) m = m->next;
    if (m)
    {
        engine->pg_message(m, message_key());
        pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
    }

    bool asap = media != nullptr || m && m->next;

    nextactiontime = ct;
    if (!asap) nextactiontime += nexttime;
}

void lan_engine::contact_s::start_media()
{
    call_status = contact_s::IN_PROGRESS;
    if (media == nullptr) media = new media_stuff_s();
}


void lan_engine::contact_s::recv()
{
    byte tmpbuf[65536];
    if (pipe.connected() && state != ROTTEN)
    {
        packet_id_e pid = pipe.packet_id();

        if (_tcp_recv_begin_ < pid  && pid < _tcp_recv_end_)
        {
#if LOGGING
            asptr pidname = pid_name(pid);
            if (pid != PID_NONE) Log("recv: %s", pidname.s);
#endif

            if (_tcp_encrypted_begin_ < pid  && pid < _tcp_encrypted_end_)
            {
                log_auth_key(pidname.s, message_key());

                if (decrypt(tmpbuf, pipe.rcvbuf, pipe.rcvbuf_sz, message_key()))
                {
                    logm("decrypt ok: %s", pidname.s);

                    stream_reader r(tmpbuf, pipe.rcvbuf_sz - crypto_secretbox_MACBYTES);
                    if (!r.end()) handle_packet(pid, r);
                } else
                {
                    logm("decrypt fail: %s", pidname.s);
                }

            } else
            {
                stream_reader r(pipe.rcvbuf, pipe.rcvbuf_sz);
                handle_packet(pid, r);
            }
        }

        if (pid != PID_NONE && pid != PID_DEAD)
            pipe.cpdone();
    }

    if (data_changed)
    {
        if (state == OFFLINE && call_status != CALL_OFF)
            stop_call_activity();

        contact_data_s cd;
        fill_data(cd);
        engine->hf->update_contact(&cd);
    }

}

void lan_engine::contact_s::stop_call_activity(bool notify_me)
{
    if (notify_me) engine->hf->message(MT_CALL_STOP, id, now(),  nullptr, 0);
    call_status = CALL_OFF;
    if (media)
    {
        delete media;
        media = nullptr;
    }
}


void lan_engine::contact_s::handle_packet( packet_id_e pid, stream_reader &r )
{
    switch (pid)
    {
    case PID_INVITE:
        {
            name = r.reads();
            str_c oim = invitemessage;
            invitemessage = r.reads();
            if (state == contact_s::ACCEPT_RESTORE_CONNECT)
            {
                state = contact_s::ACCEPT;
                key_sent = false;
                nextactiontime = time_ms();
                data_changed = false;
            } else
            {
                bool sendrq = invitemessage != oim;
                state = contact_s::INVITE_RECV;
                data_changed = true;
                contact_data_s cd;
                fill_data(cd);
                engine->hf->update_contact(&cd);
                if (sendrq)
                    engine->hf->message(MT_FRIEND_REQUEST, id, now(), invitemessage.cstr(), invitemessage.get_length());
            }
        }
        break;
    case PID_ACCEPT:
        if (const byte *key = r.read( SIZE_KEY ))
        {
            str_c rname = r.reads();
            name = rname;
            memcpy( authorized_key, key, SIZE_KEY );

            engine->pg_ready(raw_public_id, authorized_key);
            pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);

            changed_self = -1; // name, status and other data will be sent
            state = ONLINE;
            data_changed = true;

            engine->pg_time(false, authorized_key);
            pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);

        }
        break;
    case PID_REJECT:
        state = REJECTED;
        data_changed = true;
        break;
    case PID_READY:
        if (const byte *pubid = r.read(SIZE_PUBID))
        {
            if (0 == memcmp(engine->first->raw_public_id,pubid, SIZE_PUBID))
            {
                ASSERT( state == ONLINE || state == ACCEPT || state == BACKCONNECT );
                data_changed |= state != ONLINE;
                state = ONLINE;
                if (data_changed)
                {
                    nextactiontime = time_ms();
                    changed_self = -1;

                    engine->pg_time(false, authorized_key);
                    pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
                }
            }
        }
        break;
    case PID_MESSAGE:
        {
            r.readi(); // read random stub
            USHORT flags = r.readus();
            message_type_lan_e mt = (message_type_lan_e)r.readus();
            u64 dtb = r.readll(0);
            int sent = r.readi(-1);
            int len = r.readi(-1);
            time_t create_time = 0;
            if (flags & 1)
                create_time = r.readll(now()) + correct_create_time;

            USHORT msgl = r.readus();
            const byte *msg = r.read(msgl);

            if (dtb && sent >= 0 && len >= 0 && (sent + msgl <= len) && msg)
            {
                delivery_data_s & dd = delivery[ dtb ];
                if ( dd.buf.size() == 0 || sent == 0 )
                {
                    dd.buf.resize(len);
                    dd.rcv_size = 0;
                    dd.create_time = create_time;
                }
                dd.rcv_size += msgl;
                memcpy( dd.buf.data() + sent, msg, msgl );

                if (dd.rcv_size == dd.buf.size())
                {
                    // delivered full message
                    engine->pg_delivered( dtb, message_key() );
                    pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
                    pstr_c rstr; rstr.set(asptr((const char *)dd.buf.data(), dd.buf.size()));
                    switch (mt)
                    {
                        case MTL_CHANGED_NAME:
                            name = rstr;
                            if (name.get_length() == 1) name.set_length();
                            data_changed = true;
                            break;
                        case MTL_CHANGED_STATUSMSG:
                            statusmsg = rstr;
                            if (statusmsg.get_length() == 1) statusmsg.set_length();
                            data_changed = true;
                            break;
                        case MTL_OSTATE:
                            ostate = (contact_online_state_e)rstr.as_int();
                            data_changed = true;
                            break;
                        case MTL_GENDER:
                            gender = (contact_gender_e)rstr.as_int();
                            data_changed = true;
                            break;
                        case MTL_CALL:
                            if (CALL_OFF == call_status)
                            {
                                engine->hf->message(MT_INCOMING_CALL, id, dd.create_time, "audio", 5);
                                call_status = IN_CALL;
                            }
                            break;
                        case MTL_CALL_CANCEL:
                            if (CALL_OFF != call_status)
                                stop_call_activity();
                            break;
                        case MTL_CALL_ACCEPT:
                            if (OUT_CALL == call_status)
                            {
                                engine->hf->message(MT_CALL_ACCEPTED, id, dd.create_time, nullptr, 0);
                                start_media();
                            }
                            break;
                        case MTL_AUDIO_FRAME:
                            if (IN_PROGRESS == call_status && media)
                            {
                                audio_format_s fmt(AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS);
                                int sz = media->decode_audio( rstr.as_sptr().s, rstr.get_length() );
                                if (sz > 0)
                                    engine->hf->play_audio(id, &fmt, media->uncompressed.data(), sz);
                            }
                            break;
                        default:
                            {
                                //int messagetime = time_ms();
                                //if ((messagetime - engine->last_message) > 5000 || 0 != memcmp(engine->md5_of_last_message, md5.result(), 16))
                                {
                                    //engine->last_message = messagetime;
                                    if (rstr.get_length() == 1 && rstr.get_char(0) == 0)
                                        engine->hf->message((message_type_e)mt, id, dd.create_time, nullptr, 0);
                                    else
                                        engine->hf->message((message_type_e)mt, id, dd.create_time, rstr.as_sptr().s, rstr.get_length());
                                    //memcpy(engine->md5_of_last_message, md5.result(), 16);
                                }
                            }
                            break;
                    }
                    delivery.erase( dtb );
                }
            }

        }
        break;
    case PID_DELIVERED:
        if (u64 dtb = r.readll(0))
            for(msg_s *m = sendmessage_f; m; m=m->next)
                if (m->delivery_tag == dtb)
                {
                    engine->hf->delivered(m->delivery_tag);
                    LIST_DEL(m,sendmessage_f,sendmessage_l,prev,next);
                    m->die();
                    break;
                }
        break;
    case PID_TIME:
        {
            r.read(sizeof(u64)); // skip random stuff
            time_t remote_peer_time = r.readll(0);
            if (remote_peer_time)
            {
                bool resync = r.readb() != 0;
                correct_create_time = (int)((long long)now() - (long long)remote_peer_time);
                need_resync = abs(correct_create_time) > 10; // 10+ seconds time delta :( resync it every 2 min
                if (need_resync) next_sync = time_ms() + 120000;
                if (resync)
                {
                    engine->pg_time(false, authorized_key);
                    pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
                }
            }
        }
        break;
    }

}

void lan_engine::file_send(int cid, const file_send_info_s *finfo)
{
}

void lan_engine::file_control(u64, file_control_e)
{
}
void lan_engine::file_portion(u64, const file_portion_s *)
{

}


//unused
void lan_engine::proxy_settings(int /*proxy_type*/, const char * /*proxy_address*/)
{
}