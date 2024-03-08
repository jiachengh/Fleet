#ifndef ART_RUNTIME_NIEL_TRACE_H_
#define ART_RUNTIME_NIEL_TRACE_H_

#define NIEL_TRACE_ENABLED true

#if NIEL_TRACE_ENABLED
#define NIEL_TRACE_GET_FIELD(obj) niel::trace::TraceGetField(obj)
#define NIEL_TRACE_SET_FIELD(obj) niel::trace::TraceSetField(obj)
#define NIEL_TRACE_GET_FIELD_OBJECT(obj, ref) niel::trace::TraceGetFieldObject(obj, ref)
#define NIEL_TRACE_SET_FIELD_OBJECT(obj, ref) niel::trace::TraceSetFieldObject(obj, ref)
#else
#define NIEL_TRACE_GET_FIELD(obj)
#define NIEL_TRACE_SET_FIELD(obj)
#define NIEL_TRACE_GET_FIELD_OBJECT(obj, ref)
#define NIEL_TRACE_SET_FIELD_OBJECT(obj, ref)
#endif

namespace art {

namespace mirror {
    class Object;
}

namespace niel {

namespace trace {

void InitializeTrace();
void TraceGetField(mirror::Object * obj);
void TraceGetFieldObject(mirror::Object * obj, mirror::Object * ref);
void TraceSetField(mirror::Object * obj);
void TraceSetFieldObject(mirror::Object * obj, mirror::Object * ref);

}
}
}

#endif // ART_RUNTIME_NIEL_TRACE_H_