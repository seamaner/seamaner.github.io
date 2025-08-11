---
layout: page
title: About
description: 学习：知与行
keywords: seamaner
comments: true
menu: 关于
permalink: /about/
---
大家好，我是seamaner

系统工程师;  

自封的安全专家(kernel CVEer);  

自封的Hacker(pwn.college蓝带);  

学习的过程也是不断思考的过程，记录思考的有趣的地方。同时分享在这里，如果能同时帮助到同在思考的或者困惑的，那自然更好。但愿如此。


## 联系

<ul>
{% for website in site.data.social %}
<li>{{website.sitename }}：<a href="{{ website.url }}" target="_blank">@{{ website.name }}</a></li>
{% endfor %}
{% if site.url contains 'seamaner.io' %}
{% endif %}
</ul>


## Skill Keywords

{% for skill in site.data.skills %}
### {{ skill.name }}
<div class="btn-inline">
{% for keyword in skill.keywords %}
<button class="btn btn-outline" type="button">{{ keyword }}</button>
{% endfor %}
</div>
{% endfor %}

