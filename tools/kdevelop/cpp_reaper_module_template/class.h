////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////


{% block include_guard_open %}
#ifndef REAPER_{% for ns in namespaces %}{{ ns|upper }}_{% endfor %}{{ name|upper }}MODULE_INCLUDED
#define REAPER_{% for ns in namespaces %}{{ ns|upper }}_{% endfor %}{{ name|upper }}MODULE_INCLUDED
{% endblock include_guard_open %}


#include "game/ModuleUpdater.h"


struct {{ name }}ModuleDescriptor : public ModuleDescriptor {


};


struct {{ name }}Module {


};


class {{ name }}Updater : public ModuleUpdater<{{ name }}Module, {{ name }}ModuleDescriptor>
{
    using parent_type = ModuleUpdater<{{ name }}Module, {{ name }}ModuleDescriptor>;
public:
    {{ name }}Updater(AbstractWorldUpdater* worldUpdater) : parent_type(worldUpdater) {}


public:
    void update(float dt);
    void createModule(EntityId id, const {{ name }}ModuleDescriptor* descriptor) override;
};


{% block include_guard_close %}
#endif // REAPER_{% for ns in namespaces %}{{ ns|upper }}_{% endfor %}{{ name|upper }}MODULE_INCLUDED
{% endblock include_guard_close %}
