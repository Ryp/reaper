////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MODULEUPDATER_INCLUDED
#define REAPER_MODULEUPDATER_INCLUDED

#include <map>

class AbstractWorldUpdater;

using EntityId = u32;

template <class ModuleType>
class ModuleAccessor
{
public:
    ModuleAccessor(std::map<EntityId, ModuleType>& modules) : _modules(modules) {}

    std::map<EntityId, ModuleType> const& map() const { return _modules; }

    ModuleType* operator[](EntityId id)
    {
        Assert(_modules.count(id) > 0, "entity id not found");
        return &_modules.at(id);
    }

    ModuleType const* operator[](EntityId id) const
    {
        Assert(_modules.count(id) > 0, "entity id not found");
        return &_modules.at(id);
    }

private:
    std::map<EntityId, ModuleType>& _modules;
};

template <class ModuleType>
class ModuleUpdater
{
public:
    ModuleUpdater(AbstractWorldUpdater* worldUpdater) : _worldUpdater(worldUpdater) {}
    virtual ~ModuleUpdater() {}

public:
    ModuleAccessor<ModuleType> getModuleAccessor() { return ModuleAccessor<ModuleType>(_modules); }
    void removeModule(EntityId id) { if (_modules.count(id) > 0) _modules.erase(id); }

protected:
    void addModule(EntityId id, ModuleType& module)
    {
        Assert(_modules.count(id) == 0, "entity id already taken");
        _modules.emplace(std::pair<EntityId, ModuleType>(id, module));
    }

protected:
    AbstractWorldUpdater*          _worldUpdater;
    std::map<EntityId, ModuleType> _modules;
};

#endif // REAPER_MODULEUPDATER_INCLUDED
