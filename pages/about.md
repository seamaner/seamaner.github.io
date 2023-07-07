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

思考让人觉醒，醒悟使人进步。
定制化程度高，可能是安全产品利润率较低的一个原因，每个定制版本使用的复制数较低，研发成本得不到有效摊薄；windows10/windows11需要不同定制的，会倒逼研发采用更简单易用的技术以降低成本。（EDR）


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
