////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////


{% block include_guard_open %}
#ifndef REAPER_{% for ns in namespaces %}{{ ns|upper }}_{% endfor %}{{ name|upper }}_INCLUDED
#define REAPER_{% for ns in namespaces %}{{ ns|upper }}_{% endfor %}{{ name|upper }}_INCLUDED
{% endblock include_guard_open %}


{% block includes %}
{% for included_file in included_files %}
#include {{ included_file }}
{% endfor %}
{% endblock includes %}


{% block namespaces_open %}
{% include "namespace_open_cpp.txt" %}
{% endblock namespaces_open %}


{% block forward_declarations %}
{% endblock forward_declarations %}


{% block class_declaration_open %}
class {{ name }}{% if base_classes %} :{% for base in base_classes %} {{ base.inheritanceMode }} {{ base.baseType }}{% if not forloop.last %},{% endif %}{% endfor %}{% endif %}
{
{% endblock class_declaration_open %}

{% block class_body %}
public:
    {{ name }}() = default;
    ~{{ name }}() = default;


    {{ name }}(const {{ name }}& other) = delete;
    {{ name }}& operator=(const {{ name }}& other) = delete;
{% if public_members or public_functions %}


public:
{% endif %}
{% if public_functions %}
    {% for method in public_functions %}
{% include "method_declaration_cpp.txt" %}
    {% endfor %}
{% endif %}
{% if public_members %}
    {% for member in public_members %}
    {{ member.type }} {{ member.name }};
    {% endfor %}
{% endif %}
{% if protected_members or protected_functions %}


protected:
{% endif %}
{% if protected_functions %}
    {% for method in protected_functions %}
{% include "method_declaration_cpp.txt" %}
    {% endfor %}
{% endif %}
{% if protected_members %}
    {% for member in protected_members %}
    {{ member.type }} {{ member.name }};
    {% endfor %}
{% endif %}
{% if private_members or private_functions %}


private:
{% endif %}
{% if private_functions %}
    {% for method in private_functions %}
{% include "method_declaration_cpp.txt" %}
    {% endfor %}
{% endif %}
{% for member in private_members %}
    {{ member.type }} {{ member.name }};
{% endfor %}
{% endblock class_body %}
{% block class_bottom %}
{% endblock %}
{% block class_declaration_close %}
};
{% endblock %}

{% block outside_class %}
{% endblock %}

{% block namespaces_close %}
{% include "namespace_close_cpp.txt" %}
{% endblock namespaces_close %}

{% block outside_namespace %}
{% endblock %}


{% block include_guard_close %}
#endif // REAPER_{% for ns in namespaces %}{{ ns|upper }}_{% endfor %}{{ name|upper }}_INCLUDED
{% endblock include_guard_close %}
