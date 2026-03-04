---
layout: page
title: About
description: 学习：知与行
keywords: seamaner
comments: true
menu: 关于
permalink: /about/
---
  
pwn.college蓝带[seamaner](https://pwn.college/hacker/45139);
   
kernel CVE report & fix(CVE-2025-38184);  

Wiz Bug Bounty Masterclass Certificate[seamaner](https://www.wiz.io/bug-bounty-masterclass/certificate/417786a8-55b5-490e-a6b7-1dc615641f6e);
   
   

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



