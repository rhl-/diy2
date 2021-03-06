#ifndef DIY_PROXY_HPP
#define DIY_PROXY_HPP


namespace diy
{
  struct Master::Proxy
  {
    template <class T>
    struct EnqueueIterator;

                        Proxy(Master* master, int gid):
                          gid_(gid),
                          master_(master),
                          incoming_(&master->incoming(gid)),
                          outgoing_(&master->outgoing(gid)),
                          collectives_(&master->collectives(gid))       {}

    int                 gid() const                                     { return gid_; }

    //! Enqueue data whose size can be determined automatically
    template<class T>
    void                enqueue(const BlockID&  to,                                     //!< target block (gid,proc)
                                const T&        x,                                      //!< data (eg. STL vector)
                                void (*save)(BinaryBuffer&, const T&) = &::diy::save<T> //!< optional serialization function
                               ) const
    { OutgoingQueues& out = *outgoing_; save(out[to], x); }

    //! Enqueue an array of data whose size is given explicitly
    template<class T>
    void                enqueue(const BlockID&  to,                                     //!< target block (gid,proc)
                                const T*        x,                                      //!< pointer to the data (eg. address of start of vector)
                                size_t          n,                                      //!< size in data elements (eg. ints)
                                void (*save)(BinaryBuffer&, const T&) = &::diy::save<T> //!< optional serialization function
                               ) const;

    //! Dequeue data whose size can be determined automatically.
    //! Diy will allocate the receive buffer.
    template<class T>
    void                dequeue(int             from,                                   //!< target block gid
                                T&              x,                                      //!< data (eg. STL vector)
                                void (*load)(BinaryBuffer&, T&) = &::diy::load<T>       //!< optional serialization function
                               ) const
    { IncomingQueues& in  = *incoming_; load(in[from], x); }

    //! Dequeue an array of data whose size is given explicitly.
    //! The user needs to allocate the receive buffer.
    template<class T>
    void                dequeue(int             from,                                   //!< target block gid
                                T*              x,                                      //!< pointer to the data (eg. address of start of vector)
                                size_t          n,                                      //!< size in data elements (eg. ints)
                                void (*load)(BinaryBuffer&, T&) = &::diy::load<T>       //!< optional serialization function
                               ) const;

    template<class T>
    EnqueueIterator<T>  enqueuer(const T& x,
                                 void (*save)(BinaryBuffer&, const T&) = &::diy::save<T>) const
    { return EnqueueIterator<T>(this, x, save); }

    IncomingQueues*     incoming() const                                { return incoming_; }
    MemoryBuffer&       incoming(int from) const                        { return (*incoming_)[from]; }
    inline void         incoming(std::vector<int>& v) const;            // fill v with every gid from which we have a message

    OutgoingQueues*     outgoing() const                                { return outgoing_; }
    MemoryBuffer&       outgoing(const BlockID& to) const               { return (*outgoing_)[to]; }

    template<class T, class Op>
    inline void         all_reduce(const T& in, Op op) const;
    template<class T>
    inline T            read() const;
    template<class T>
    inline T            get() const;

    template<class T>
    inline void         scratch(const T& in) const;

    CollectivesList*    collectives() const                             { return collectives_; }

    Master*             master() const                                  { return master_; }

    private:
      int               gid_;
      Master*           master_;
      IncomingQueues*   incoming_;
      OutgoingQueues*   outgoing_;
      CollectivesList*  collectives_;
  };

  template<class T>
  struct Master::Proxy::EnqueueIterator:
    public std::iterator<std::output_iterator_tag, void, void, void, void>
  {
    typedef     void (*SaveT)(BinaryBuffer&, const T&);

                        EnqueueIterator(const Proxy* proxy, const T& x,
                                        SaveT save = &::diy::save<T>):
                            proxy_(proxy), x_(x), save_(save)               {}

    EnqueueIterator&    operator=(const BlockID& to)                        { proxy_->enqueue(to, x_, save_); return *this; }
    EnqueueIterator&    operator*()                                         { return *this; }
    EnqueueIterator&    operator++()                                        { return *this; }
    EnqueueIterator&    operator++(int)                                     { return *this; }

    private:
      const Proxy*  proxy_;
      const T&      x_;
      SaveT         save_;

  };

  struct Master::ProxyWithLink: public Master::Proxy
  {
            ProxyWithLink(const Proxy&    proxy,
                          void*           block,
                          Link*           link):
              Proxy(proxy),
              block_(block),
              link_(link)                                           {}

      Link*   link() const                                          { return link_; }
      void*   block() const                                         { return block_; }

    private:
      void*   block_;
      Link*   link_;
  };
}


void
diy::Master::Proxy::
incoming(std::vector<int>& v) const
{
  for (IncomingQueues::const_iterator it = incoming_->begin(); it != incoming_->end(); ++it)
    v.push_back(it->first);
}

template<class T, class Op>
void
diy::Master::Proxy::
all_reduce(const T& in, Op op) const
{
  collectives_->push_back(Collective(new detail::AllReduceOp<T,Op>(in, op)));
}

template<class T>
T
diy::Master::Proxy::
read() const
{
  T res;
  collectives_->front().result_out(&res);
  return res;
}

template<class T>
T
diy::Master::Proxy::
get() const
{
  T res = read<T>();
  collectives_->pop_front();
  return res;
}

template<class T>
void
diy::Master::Proxy::
scratch(const T& in) const
{
  collectives_->push_back(Collective(new detail::Scratch<T>(in)));
}

template<class T>
void
diy::Master::Proxy::
enqueue(const BlockID& to, const T* x, size_t n,
        void (*save)(BinaryBuffer&, const T&)) const
{
    OutgoingQueues& out = *outgoing_;
    BinaryBuffer&   bb  = out[to];
    if (save == (void (*)(BinaryBuffer&, const T&)) &::diy::save<T>)
        diy::save(bb, x, n);       // optimized for unspecialized types
    else
        for (size_t i = 0; i < n; ++i)
            save(bb, x[i]);
}

template<class T>
void
diy::Master::Proxy::
dequeue(int from, T* x, size_t n,
        void (*load)(BinaryBuffer&, T&)) const
{
    IncomingQueues& in = *incoming_;
    BinaryBuffer&   bb = in[from];
    if (load == (void (*)(BinaryBuffer&, T&)) &::diy::load<T>)
        diy::load(bb, x, n);       // optimized for unspecialized types
    else
        for (size_t i = 0; i < n; ++i)
            load(bb, x[i]);
}


#endif
