#include "vast/ingestor.h"

#include "vast/exception.h"
#include "vast/event_source.h"
#include "vast/logger.h"
#include "vast/segment.h"
#include "vast/source/broccoli.h"
#include "vast/source/file.h"

namespace vast {

ingestor::ingestor(cppa::actor_ptr tracker,
                   cppa::actor_ptr archive,
                   cppa::actor_ptr index)
  : archive_(archive)
  , index_(index)
{
  // FIXME: make batch size configurable.
  size_t batch_size = 1000;

  LOG(verbose, ingest) << "spawning ingestor @" << id();

  using namespace cppa;
  chaining(false);
  init_state = (
      on(atom("initialize"), arg_match) >> [=](size_t max_events_per_chunk,
                                               size_t max_segment_size)
      {
        max_events_per_chunk_ = max_events_per_chunk;
        max_segment_size_ = max_segment_size;
      },
      on(atom("ingest"), atom("broccoli"), arg_match) >>
        [=](std::string const& host, unsigned port,
            std::vector<std::string> const& events)
      {
        broccoli_ = spawn<source::broccoli>(tracker, archive_);
        send(broccoli_,
             atom("initialize"),
             max_events_per_chunk_,
             max_segment_size_);

        for (auto& e : events)
          send(broccoli_, atom("subscribe"), e);

        send(broccoli_, atom("bind"), host, port);
      },
      on(atom("ingest"), "bro15conn", arg_match) >> [=](std::string const& filename)
      {
        auto src = spawn<source::bro15conn>(self, tracker, filename);
        send(src, atom("initialize"), max_events_per_chunk_, max_segment_size_);
        send(src, atom("extract"), batch_size);
        sources_.push_back(src);
      },
      on(atom("ingest"), "bro2", arg_match) >> [=](std::string const& filename)
      {
        auto src = spawn<source::bro2>(self, tracker, filename);
        send(src, atom("initialize"), max_events_per_chunk_, max_segment_size_);
        send(src, atom("extract"), batch_size);
        sources_.push_back(src);
      },
      on(atom("ingest"), val<std::string>, arg_match) >> [=](std::string const&)
      {
        LOG(error, ingest) << "invalid ingestion file type";
      },
      on(atom("source"), atom("ack"), arg_match) >> [=](size_t extracted)
      {
        reply(atom("extract"), batch_size);
      },
      on(atom("source"), atom("done"), arg_match) >> [=](size_t extracted)
      {
        remove(last_sender());
      },
      on_arg_match >> [=](segment const& /* s */)
      {
        index_ << last_dequeued();
        archive_ << last_dequeued();
      },
      on(atom("shutdown")) >> [=]
      {
        terminating_ = true;

        if (broccoli_)
          broccoli_ << last_dequeued();

        if (sources_.empty())
          shutdown();

        for (auto src : sources_)
          remove(src);
      });
}

void ingestor::remove(cppa::actor_ptr src)
{
  auto i = std::find(sources_.begin(), sources_.end(), src);
  assert(i != sources_.end());
  sources_.erase(i);

  using namespace cppa;
  handle_response(sync_send(src, atom("shutdown")))(
    on(arg_match) >> [=](segment const& s)
    {
      DBG(ingest) << "ingestor @" << id()
        << " received last segment @" << s.id()
        << " from @" << last_sender()->id();

      index_ << last_dequeued();
      archive_ << last_dequeued();
      if (sources_.empty() && terminating_)
        shutdown();
    },
    on(atom("done")) >> [=]
    {
      DBG(ingest) << "ingestor @" << id()
        << " received done message"
        << " from @" << last_sender()->id();

      if (sources_.empty() && terminating_)
        shutdown();
    },
    after(std::chrono::seconds(3)) >> [=]
    {
      LOG(error, ingest)
        << "event source @" << src->id()
        << " terminated, but could not reliable shutdown"
        << " @" << last_sender()->id()
        << ", buffered events may be lost";

      if (sources_.empty() && terminating_)
        shutdown();
    });
}

void ingestor::shutdown()
{
  quit();
  LOG(verbose, ingest) << "ingestor @" << id() << " termianted";
}

} // namespace vast
