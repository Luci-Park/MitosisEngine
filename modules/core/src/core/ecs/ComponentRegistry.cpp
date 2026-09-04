/**
 * @file ComponentRegistry.cpp
 * @author Sumin Park
 * @brief Name -> component operations, for callers that have no C++ type
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "core/ecs/ComponentRegistry.h"

#include "core/ecs/components/Transform.h"
#include "core/ecs/components/WorldTransform.h"
#include "core/log/Assert.h"

#include <algorithm>

namespace mts
{
    namespace
    {
        // alignment is always a power of two, so the mask form is exact
        uint32_t AlignUp(uint32_t n, uint32_t alignment)
        {
            return (n + alignment - 1) & ~(alignment - 1);
        }

        // -- the erased operations, shared by every script-declared component --
        //
        // One set of functions for all of them, rather than one instantiation
        // per type: everything that varies is already in the ComponentOps they
        // are handed.

        void *RuntimeGet(const ComponentOps &ops, World &world, Entity entity)
        {
            return world.GetRaw(entity, ops.mType);
        }

        bool RuntimeHas(const ComponentOps &ops, const World &world, Entity entity)
        {
            return world.HasRaw(entity, ops.mType);
        }

        void RuntimeAddCopy(const ComponentOps &ops, World &world, Entity entity, const void *value)
        {
            if (!world.IsAlive(entity))
                return;

            if (void *existing = world.GetRaw(entity, ops.mType))
                std::memcpy(existing, value, ops.mSize);
            else
                world.AddRaw(entity, ops.mType, ops.mSize, ops.mAlign, value);
        }

        void RuntimeRemove(const ComponentOps &ops, World &world, Entity entity)
        {
            if (world.IsAlive(entity) && world.HasRaw(entity, ops.mType))
                world.RemoveRaw(entity, ops.mType);
        }

        void RuntimeDeferAdd(const ComponentOps &ops, CommandBuffer &commands, Entity entity, const void *value)
        {
            commands.AddRaw(entity, ops.mType, ops.mSize, ops.mAlign, value);
        }

        void RuntimeDeferRemove(const ComponentOps &ops, CommandBuffer &commands, Entity entity)
        {
            commands.RemoveRaw(entity, ops.mType);
        }

        /// Whether a re-declaration describes the same component. Names and
        /// kinds in order, because the offsets are derived from exactly that.
        bool SameLayout(const ComponentOps &existing, std::span<const RuntimeFieldDecl> fields)
        {
            if (existing.mFields.size() != fields.size())
                return false;

            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                if (existing.mFields[i].mName != fields[i].mName || existing.mFields[i].mKind != fields[i].mKind)
                    return false;
            }
            return true;
        }
    }

    ComponentRegistry &ComponentRegistry::Instance()
    {
        static ComponentRegistry registry;
        return registry;
    }

    std::string_view ComponentRegistry::Intern(std::string_view name)
    {
        mInternedNames.emplace_back(name);
        return mInternedNames.back();
    }

    ComponentOps *ComponentRegistry::FindChecked(uint32_t hash, std::string_view name)
    {
        const auto it = mByHash.find(hash);
        if (it == mByHash.end())
            return nullptr;

        // The debug-only check in TypeIdOf is not enough any more: a name can
        // now arrive from a data file, so a collision is a shipping-build data
        // bug and silently aliasing two components would be far worse than
        // stopping.
        MTS_CHECK(it->second->mType.name == name,
                  "ComponentRegistry: name hash collision - \"{}\" and \"{}\" both hash to {}; "
                  "component names must be globally unique",
                  it->second->mType.name, name, hash);

        return it->second;
    }

    uint32_t ComponentRegistry::SeqForHash(uint32_t hash, std::string_view name)
    {
        if (const ComponentOps *existing = FindChecked(hash, name))
            return existing->mType.seq;

        const uint32_t seq = NextSeq();
        MTS_CHECK(seq < kMaxComponentTypes,
                  "ComponentRegistry: \"{}\" would be component type {}, past kMaxComponentTypes ({}). "
                  "Raise kMaxComponentTypes in Signature.h, or declare fewer component types.",
                  name, seq, kMaxComponentTypes);
        return seq;
    }

    const ComponentOps &ComponentRegistry::InsertNative(const ComponentOps &ops, const void *defaultValue)
    {
        if (ComponentOps *existing = FindChecked(ops.mType.hash, ops.mType.name))
        {
            MTS_CHECK(!existing->mRuntime,
                      "ComponentRegistry: \"{}\" was already declared by a script. Register the C++ "
                      "components before loading any script.",
                      ops.mType.name);

            // MTS_CHECK, not MTS_ASSERT. TypeId::name is the *bare* name -
            // BareNameOffset strips the namespace - so a::Foo and b::Foo carry
            // the same name and the same hash, and sail through FindChecked's
            // name comparison. Compiled out, this would hand the second
            // registration the first one's entry, whose thunks are instantiated
            // for the wrong type: mAddCopy calls AddComponent<a::Foo> and the
            // field thunks static_cast bytes that are really a b::Foo. Silent
            // type confusion is not something to leave to Debug.
            MTS_CHECK(existing->mType.seq == ops.mType.seq && existing->mSize == ops.mSize,
                      "ComponentRegistry: two different components are both named \"{}\". TypeId hashes "
                      "the bare name, so component names must be unique across namespaces.",
                      ops.mType.name);

            // A registration that arrives with a field table wins over one that
            // did not have it. Registration is idempotent by design, so the
            // order two callers happen to run in should not decide whether a
            // component's values are reachable by name - and silently having no
            // fields is a failure with nothing to notice it by. Two *different*
            // non-empty tables is a real disagreement.
            if (existing->mFields.empty())
            {
                existing->mFields = ops.mFields;
            }
            else
            {
                // Also MTS_CHECK: whichever call ran first would otherwise win
                // silently, which is the same "failure with nothing to notice it
                // by" the paragraph above argues against.
                MTS_CHECK(ops.mFields.empty() || ops.mFields.data() == existing->mFields.data(),
                          "ComponentRegistry: \"{}\" registered twice with different field tables",
                          ops.mType.name);
            }

            return *existing;
        }

        MTS_CHECK(ops.mType.seq < kMaxComponentTypes,
                  "ComponentRegistry: \"{}\" is component type {}, past kMaxComponentTypes ({}). "
                  "Raise kMaxComponentTypes in Signature.h.",
                  ops.mType.name, ops.mType.seq, kMaxComponentTypes);

        // Published before the entry is reachable, so no erased caller can hold
        // this TypeId before the mask knows what it is. This is the only place
        // storage kind and seq are both in hand for a type someone may later
        // name at runtime.
        if (ops.mStorage == StorageKind::SparseSet)
            NoteSparseComponentSeq(ops.mType.seq);

        mDefaultValues.emplace_back(ops.mSize);
        std::memcpy(mDefaultValues.back().data(), defaultValue, ops.mSize);

        mOps.push_back(ops);
        ComponentOps *stored = &mOps.back();
        stored->mDefaultValue = std::span<const std::byte>(mDefaultValues.back());

        mByHash.emplace(ops.mType.hash, stored);
        mBySeq.emplace(ops.mType.seq, stored);
        return *stored;
    }

    const ComponentOps &ComponentRegistry::RegisterRuntime(std::string_view name,
                                                           std::span<const RuntimeFieldDecl> fields)
    {
        MTS_CHECK(!name.empty(), "ComponentRegistry::RegisterRuntime: component name is empty");

        const uint32_t hash = Fnv1a32(name);

        if (ComponentOps *existing = FindChecked(hash, name))
        {
            MTS_CHECK(existing->mRuntime,
                      "ComponentRegistry: \"{}\" is a C++ component; a script may not redeclare it", name);

            // The hot-reload path. Same fields means the same layout, so every
            // archetype already built out of this component still means what it
            // meant and the reload is free.
            MTS_CHECK(SameLayout(*existing, fields),
                      "ComponentRegistry: \"{}\" is already declared with a different field list. "
                      "Live archetypes hold rows of the old layout, and migrating them is not "
                      "implemented - restart the world to change a component's fields.",
                      name);
            return *existing;
        }

        // Fields are laid out in declaration order rather than sorted by
        // alignment: a script author can predict the result, and the padding a
        // reorder would save is not worth a layout that changes when a field is
        // renamed.
        std::vector<FieldDesc> descs;
        descs.reserve(fields.size());

        uint32_t offset = 0;
        uint32_t maxAlign = 1;

        for (const RuntimeFieldDecl &decl : fields)
        {
            MTS_CHECK(!decl.mName.empty(), "ComponentRegistry: \"{}\" has a field with an empty name", name);
            for (const FieldDesc &seen : descs)
            {
                MTS_CHECK(seen.mName != decl.mName, "ComponentRegistry: \"{}\" declares field \"{}\" twice", name,
                          decl.mName);
            }

            const uint32_t align = FieldAlign(decl.mKind);
            offset = AlignUp(offset, align);

            FieldDesc desc{};
            desc.mName = Intern(decl.mName);
            desc.mKind = decl.mKind;
            desc.mOffset = offset;
            // mGet and mSet stay null: a script component is plain data with no
            // invariant to protect, so a memcpy at the offset is both correct
            // and the cheapest thing available
            descs.push_back(desc);

            offset += FieldSize(decl.mKind);
            maxAlign = std::max(maxAlign, align);
        }

        // A fieldless tag still needs one byte: ComponentColumn::Count divides
        // the byte count by the element size.
        const uint32_t size = std::max(AlignUp(offset, maxAlign), 1u);

        const uint32_t seq = SeqForHash(hash, name);

        mRuntimeFields.push_back(std::move(descs));
        mDefaultValues.emplace_back(size); // value-initialised: a script component defaults to zeroes

        // ...except EntityRef, whose "unset" value is kNullEntity, not zero
        // bytes. Entity's null sentinel is mIndex == UINT32_MAX (Entity.h), so
        // a zeroed field reads back as a handle to slot 0 generation 0 - a
        // reference that looks live instead of one that looks unset.
        for (const FieldDesc &desc : mRuntimeFields.back())
        {
            if (desc.mKind == FieldKind::EntityRef)
                std::memcpy(mDefaultValues.back().data() + desc.mOffset, &kNullEntity, sizeof(Entity));
        }

        ComponentOps ops{};
        ops.mType = TypeId{seq, hash, Intern(name)};
        ops.mSize = size;
        ops.mAlign = maxAlign;
        ops.mStorage = StorageKind::Table;
        ops.mRuntime = true;
        ops.mGet = &RuntimeGet;
        ops.mHas = &RuntimeHas;
        ops.mAddCopy = &RuntimeAddCopy;
        ops.mRemove = &RuntimeRemove;
        ops.mDeferAdd = &RuntimeDeferAdd;
        ops.mDeferRemove = &RuntimeDeferRemove;
        ops.mFields = std::span<const FieldDesc>(mRuntimeFields.back());
        ops.mDefaultValue = std::span<const std::byte>(mDefaultValues.back());

        mOps.push_back(ops);
        ComponentOps *stored = &mOps.back();

        mByHash.emplace(hash, stored);
        mBySeq.emplace(seq, stored);
        return *stored;
    }

    const ComponentOps *ComponentRegistry::Find(std::string_view name) const
    {
        const ComponentOps *ops = FindByHash(Fnv1a32(name));

        // a colliding name reports "no such component" rather than handing back
        // the wrong one; RegisterRuntime is where a collision stops the process
        return (ops != nullptr && ops->mType.name == name) ? ops : nullptr;
    }

    const ComponentOps *ComponentRegistry::FindByHash(uint32_t hash) const
    {
        const auto it = mByHash.find(hash);
        return it == mByHash.end() ? nullptr : it->second;
    }

    const ComponentOps *ComponentRegistry::FindBySeq(uint32_t seq) const
    {
        const auto it = mBySeq.find(seq);
        return it == mBySeq.end() ? nullptr : it->second;
    }

    void RegisterCoreComponents()
    {
        ComponentRegistry &registry = ComponentRegistry::Instance();

        registry.Register<Transform>(kTransformFields);
        registry.Register<WorldTransform>(kWorldTransformFields);
    }
}
