import lldb

def __lldb_init_module(debugger, internal_dict):
    def add_summary(type_name, summary_name, templated=True):
        command = None
        if templated:
            command = f'type summary add -x "^{type_name}" -F {__name__}.{summary_name} -w KryneEngine'
        else:
            command = f'type synthetic add {type_name} -F {__name__}.{summary_name} -w KryneEngine'
        debugger.HandleCommand(command)

    def add_synthetic_children_provider(type_name, provider_name, templated=True):
        command = None
        if templated:
            command = f'type synthetic add -x "^{type_name}" --python-class {__name__}.{provider_name} -w KryneEngine'
        else:
            command = f'type synthetic add {type_name} --python-class {__name__}.{provider_name} -w KryneEngine'
        debugger.HandleCommand(command)

    add_summary("KryneEngine::DynamicArray<.*>", dynamic_array_summary.__name__)
    add_synthetic_children_provider("KryneEngine::DynamicArray<.*>", DynamicArrayChildrenProvider.__name__)

    add_summary("KryneEngine::Math::Vector2Base<.*>", vector_base_summary.__name__)
    add_summary("KryneEngine::Math::Vector3Base<.*>", vector_base_summary.__name__)
    add_summary("KryneEngine::Math::Vector4Base<.*>", vector_base_summary.__name__)
    for type in ["float", "double", "int", "uint"]:
        for count in [2, 3, 4]:
            add_summary(f"KryneEngine::{type}{count}", vector_base_summary.__name__)
            add_summary(f"KryneEngine::{type}{count}_simd", vector_base_summary.__name__)

    add_synthetic_children_provider("KryneEngine::FlatHashMap<.*>", FlatHashMapChildrenProvider.__name__)
    add_summary("KryneEngine::FlatHashMap<.*>::kvp", flat_hash_map_kvp_summary.__name__)
    add_synthetic_children_provider("KryneEngine::FlatHashMap<.*>::kvp", FlatHashMapKvpChildrenProvider.__name__)

    add_summary("KryneEngine::StringHash", string_hash_summary.__name__)
    add_summary("KryneEngine::StringViewHash", string_view_hash_summary.__name__)

    debugger.HandleCommand("type category enable KryneEngine")

def dynamic_array_summary(value_object, internal_dict):
    return f'size={value_object.GetChildMemberWithName("[size]").GetValueAsSigned()}'

class DynamicArrayChildrenProvider:
    def __init__(self, value_object, internal_dict):
        self.value_object = value_object
        self.count = 0
        self.size = None
        self.data_type = None
        self.data_size = None
        self.data_ptr = None
        self.update()

    def num_children(self):
        return self.count + 1

    def get_child_index(self, name: str):
        try:
            return 1 + int(name.lstrip('[').rstrip(']'))
        except:
            return 0 if name == "[size]" else -1

    def get_child_at_index(self, index):
        if index == 0:
            return self.size.Clone("[size]")
        elif index - 1 < self.count:
            offset = self.data_size * (index - 1)
            return self.data_ptr.CreateChildAtOffset(f'[{index - 1}]', offset, self.data_type)
        return None

    def update(self):
        self.size = self.value_object.GetChildMemberWithName("m_count")
        self.count = self.size.GetValueAsSigned()

        self.data_type = self.value_object.GetType().GetTemplateArgumentType(0)
        self.data_size = self.data_type.GetByteSize()
        self.data_ptr = self.value_object.GetChildMemberWithName("m_array")

    def has_children(self):
        return self.count > 0

def vector_base_summary(value_object, internal_dict):
    x = value_object.GetChildMemberWithName("x").GetValue()
    y = value_object.GetChildMemberWithName("y").GetValue()
    if value_object.GetChildMemberWithName("z").IsValid():
        z = value_object.GetChildMemberWithName("z").GetValue()
        if value_object.GetChildMemberWithName("w").IsValid():
            w = value_object.GetChildMemberWithName("w").GetValue()
            return f'({x}, {y}, {z}, {w})'
        return f'({x}, {y}, {z})'
    return f'({x}, {y})'

class FlatHashMapChildrenProvider:
    def __init__(self, value_object, internal_dict):
        self.value_object = value_object
        self.capacity = 0
        self.count = 0
        self.capacity_obj = None
        self.count_obj = None
        self.occupied_slots = []
        self.update()

    def num_children(self):
        return self.count + 2 # [count], [capacity] and elements

    def get_child_index(self, name: str):
        try:
            index = int(name.lstrip('[').rstrip(']'))
            for i in range(len(self.occupied_slots)):
                v_index, _ = self.occupied_slots[i]
                if v_index == index:
                    return i + 2
            return -1
        except:
            return 0 if name == "[count]" else (1 if name == "[capacity]" else -1)

    def get_child_at_index(self, index):
        if index == 0:
            return self.count_obj.Clone("[count]")
        elif index == 1:
            return self.capacity_obj.Clone("[capacity]")
        elif index - 2 < self.capacity:
            i, value = self.occupied_slots[index - 2]
            return value.Clone(f'[{i}]')
        return None

    def update(self):
        self.count_obj = self.value_object.GetChildMemberWithName("m_count")
        self.count = self.count_obj.GetValueAsUnsigned()

        self.capacity_obj = self.value_object.GetChildMemberWithName("m_capacity")
        self.capacity = self.capacity_obj.GetValueAsUnsigned()

        self.occupied_slots = []
        control_buffer = self.value_object.GetChildMemberWithName("m_controlBuffer")
        kvp_buffer = self.value_object.GetChildMemberWithName("m_kvpBuffer")
        for i in range(self.capacity):
            control = control_buffer.GetValueForExpressionPath(f'[{i}]').GetValueAsUnsigned()
            if (control & (1 << 7)) == 0:
                self.occupied_slots.append((i, kvp_buffer.GetValueForExpressionPath(f'[{i}]')))

    def has_children(self):
        return self.count > 0

def flat_hash_map_kvp_summary(value_object, internal_dict):
    key_summary = value_object.GetChildMemberWithName("[key]").GetSummary()
    if key_summary is None:
        return None
    value_summary = value_object.GetChildMemberWithName("[value]").GetSummary()
    if value_summary is None:
        return f'{key_summary}'
    return f'{key_summary}={value_summary}'

class FlatHashMapKvpChildrenProvider:
    def __init__(self, value_object, internal_dict):
        self.value_object = value_object

    def get_child_index(self, name: str):
        return 0 if name == "[key]" else (1 if name == "[value]" else -1)

    def get_child_at_index(self, index):
        if index == 0:
            return self.value_object.GetChildMemberWithName("first").Clone("[key]")
        elif index == 1:
            return self.value_object.GetChildMemberWithName("second").Clone("[value]")
        return None

    def update(self):
        pass

    def num_children(self):
        return 2

def string_hash_summary(value_object, internal_dict):
    return value_object.GetChildMemberWithName("m_string").GetSummary()

def string_view_hash_summary(value_object, internal_dict):
    return value_object.GetChildMemberWithName("m_stringView").GetSummary()